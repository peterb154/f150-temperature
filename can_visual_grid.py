#!/usr/bin/env python3
"""
CAN Bus Visual Grid Monitor with ASCII/HEX Toggle
Real-time visual grid showing all PIDs and bytes with color-coded changes
Toggle between HEX and ASCII display to find Ford's ASCII-encoded values
Click on cells to select up to 2 for combined analysis
"""

import pygame
import serial
import threading
import time
import argparse
import sys
from collections import defaultdict

class CANVisualGrid:
    def __init__(self, port, baud_rate=115200):
        self.port = port
        self.baud_rate = baud_rate
        self.serial = None
        self.running = False
        
        # Pygame setup - use more screen space
        pygame.init()
        self.WINDOW_WIDTH = 1600  # Increased width
        self.WINDOW_HEIGHT = 900
        self.screen = pygame.display.set_mode((self.WINDOW_WIDTH, self.WINDOW_HEIGHT))
        pygame.display.set_caption("🌡️ F150 CAN Bus Visual Monitor - ASCII/HEX Explorer")
        
        # Grid settings - bigger cells
        self.CELL_WIDTH = 80  # Increased from 60
        self.CELL_HEIGHT = 24  # Increased from 20
        self.HEADER_HEIGHT = 40
        self.PID_COLUMN_WIDTH = 100  # Increased from 80
        self.START_X = self.PID_COLUMN_WIDTH
        self.START_Y = self.HEADER_HEIGHT
        
        # Display mode toggle
        self.display_mode = "HEX"  # "HEX" or "ASCII"
        
        # Colors
        self.BLACK = (0, 0, 0)
        self.WHITE = (255, 255, 255)
        self.GREEN = (0, 255, 0)
        self.BRIGHT_GREEN = (50, 255, 50)
        self.YELLOW = (255, 255, 0)
        self.BLUE = (100, 150, 255)
        self.GRAY = (128, 128, 128)
        self.DARK_GRAY = (64, 64, 64)
        self.RED = (255, 100, 100)
        self.LIGHT_BLUE = (173, 216, 230)
        self.ORANGE = (255, 165, 0)
        self.PURPLE = (255, 0, 255)
        self.CYAN = (0, 255, 255)
        self.LIGHT_GREEN = (144, 238, 144)
        
        # Fonts - bigger fonts for better readability
        self.small_font = pygame.font.Font(None, 16)
        self.tiny_font = pygame.font.Font(None, 14)
        self.header_font = pygame.font.Font(None, 18)
        self.info_font = pygame.font.Font(None, 16)
        self.big_font = pygame.font.Font(None, 20)
        
        # Data storage
        self.pid_list = []  # Ordered list of PIDs as they appear
        self.current_values = defaultdict(lambda: [0] * 8)  # [pid] = [byte0, byte1, ..., byte7]
        self.change_times = defaultdict(lambda: [0] * 8)    # [pid] = [time_byte0_changed, ...]
        self.total_messages = 0
        self.messages_per_second = 0
        self.last_fps_time = time.time()
        self.last_message_count = 0
        
        # Scroll offset for when we have too many PIDs
        self.scroll_offset = 0
        self.max_visible_rows = (self.WINDOW_HEIGHT - self.START_Y - 150) // self.CELL_HEIGHT  # Leave more space for info panel
        
        # Multi-cell inspector - can select up to 2 cells
        self.selected_cells = []  # List of (pid, byte_index) tuples, max 2
        self.info_panel_height = 120  # Increased height for more info
        
        # Temperature calibration logging
        self.calibration_mode = False
        self.calibration_data = []  # List of (actual_temp, combined_value, timestamp)
        
    def connect(self):
        """Connect to serial port"""
        try:
            self.serial = serial.Serial(self.port, self.baud_rate, timeout=0.1)
            print(f"✅ Connected to {self.port}")
            return True
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False
    
    def handle_temperature_input(self):
        """Handle temperature input from user during calibration"""
        try:
            # Get current combined value
            if len(self.selected_cells) >= 2:
                pid1, byte1_idx = self.selected_cells[0]
                pid2, byte2_idx = self.selected_cells[1]
                byte0_val = self.current_values[pid1][byte1_idx]
                byte1_val = self.current_values[pid2][byte2_idx]
                combined = (byte0_val << 8) | byte1_val
                
                # This would need to be implemented with a proper input dialog
                # For now, just show what we would do
                print(f"🌡️ Current CAN value: {combined} - Use console input for actual temperature")
        except Exception as e:
            print(f"Error getting temperature input: {e}")
    
    def calculate_temperature_formula(self):
        """Calculate linear regression formula from calibration data"""
        if len(self.calibration_data) < 2:
            return
        
        # Extract x (CAN values) and y (temperatures)
        x_vals = [point[1] for point in self.calibration_data]  # CAN values
        y_vals = [point[0] for point in self.calibration_data]  # Actual temps
        
        # Simple linear regression: y = mx + b
        n = len(self.calibration_data)
        sum_x = sum(x_vals)
        sum_y = sum(y_vals)
        sum_xy = sum(x * y for x, y in zip(x_vals, y_vals))
        sum_x2 = sum(x * x for x in x_vals)
        
        # Calculate slope (m) and intercept (b)
        m = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x)
        b = (sum_y - m * sum_x) / n
        
        print(f"\n🧮 CALCULATED FORMULA:")
        print(f"temperature_f = {m:.6f} * combined_value + {b:.2f}")
        print(f"📐 Or: temperature_f = (combined_value * {m:.6f}) + {b:.2f}")
        
        # Test the formula with collected data
        print(f"\n🧪 FORMULA TEST:")
        print("Actual | Predicted | Error")
        print("-" * 30)
        for actual_temp, can_val, _ in self.calibration_data:
            predicted = m * can_val + b
            error = abs(actual_temp - predicted)
            print(f"{actual_temp:6.1f} | {predicted:9.1f} | {error:5.1f}")
        
        # Calculate average error
        avg_error = sum(abs(temp - (m * can_val + b)) for temp, can_val, _ in self.calibration_data) / n
        print(f"\n📊 Average error: {avg_error:.2f}°F")
    
    def parse_can_message(self, line):
        """Parse CAN message from CSV format"""
        if line.startswith('#') or not line.strip():
            return None
        
        try:
            parts = line.split(',')
            if len(parts) < 8:
                return None
            
            can_id = parts[2].strip()
            if not can_id.startswith('0x'):
                return None
            
            # Parse all 8 bytes
            byte_values = []
            for i in range(8):
                if 4 + i < len(parts):
                    hex_val = parts[4 + i].strip()
                    if hex_val and hex_val.startswith('0x'):
                        byte_values.append(int(hex_val, 16))
                    else:
                        byte_values.append(0)
                else:
                    byte_values.append(0)
            
            return (can_id, byte_values)
            
        except Exception:
            return None
    
    def read_serial_data(self):
        """Read serial data in background thread"""
        last_logged_combined = None
        
        while self.running:
            if self.serial and self.serial.in_waiting:
                try:
                    line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                    result = self.parse_can_message(line)
                    
                    if result:
                        can_id, byte_values = result
                        current_time = time.time()
                        
                        # Add new PID if not seen before
                        if can_id not in self.pid_list:
                            self.pid_list.append(can_id)
                        
                        # Check for changes and update values
                        old_values = self.current_values[can_id]
                        for i, new_value in enumerate(byte_values):
                            if old_values[i] != new_value:
                                self.change_times[can_id][i] = current_time
                            self.current_values[can_id][i] = new_value
                        
                        # Log encoder values if we have 2 cells selected and values changed
                        if len(self.selected_cells) == 2:
                            pid1, byte1_idx = self.selected_cells[0]
                            pid2, byte2_idx = self.selected_cells[1]
                            
                            # Check if this update is for our selected PID
                            if can_id == pid1:
                                byte0_val = self.current_values[pid1][byte1_idx]
                                byte1_val = self.current_values[pid2][byte2_idx]
                                combined = (byte0_val << 8) | byte1_val
                                
                                # Only log if combined value changed
                                if last_logged_combined != combined:
                                    ascii0 = self.byte_to_ascii_display(byte0_val)
                                    ascii1 = self.byte_to_ascii_display(byte1_val)
                                    
                                    # Standard logging
                                    print(f"LOG: 0x{byte0_val:02X}({ascii0}) | 0x{byte1_val:02X}({ascii1}) | {combined:5d} | ASCII: '{ascii0}{ascii1}'")
                                    
                                    # Calibration mode: prompt for actual temperature
                                    if self.calibration_mode:
                                        print(f"🌡️ CALIBRATION: CAN value = {combined} | Enter actual LCD temperature (or press Enter to skip): ", end='', flush=True)
                                    
                                    last_logged_combined = combined
                        
                        self.total_messages += 1
                
                except Exception as e:
                    pass
            
            time.sleep(0.001)  # Very fast polling
    
    def byte_to_ascii_display(self, byte_value):
        """Convert byte to ASCII character if printable, otherwise show as '.'"""
        if 32 <= byte_value <= 126:  # Printable ASCII range
            return chr(byte_value)
        else:
            return '.'
    
    def is_printable_ascii(self, byte_value):
        """Check if byte value is printable ASCII"""
        return 32 <= byte_value <= 126
    
    def get_cell_color(self, pid, byte_index):
        """Get color for a cell based on selection, ASCII content, and recent changes"""
        # Check if this cell is selected
        cell = (pid, byte_index)
        if cell in self.selected_cells:
            if len(self.selected_cells) == 1:
                return self.ORANGE
            else:
                # Different colors for first and second selection
                if self.selected_cells.index(cell) == 0:
                    return self.PURPLE
                else:
                    return self.CYAN
        
        # Highlight ASCII-printable values in ASCII mode
        if self.display_mode == "ASCII":
            byte_value = self.current_values[pid][byte_index]
            if self.is_printable_ascii(byte_value):
                # Light green background for printable ASCII
                change_time = self.change_times[pid][byte_index]
                if change_time > 0 and time.time() - change_time < 2.0:
                    return self.BRIGHT_GREEN  # Recent change takes priority
                else:
                    return self.LIGHT_GREEN
        
        change_time = self.change_times[pid][byte_index]
        if change_time == 0:
            return self.WHITE
        
        age = time.time() - change_time
        
        if age < 0.5:  # Bright green for very recent changes
            return self.BRIGHT_GREEN
        elif age < 1.0:  # Green for recent changes
            return self.GREEN
        elif age < 2.0:  # Yellow for somewhat recent
            return self.YELLOW
        elif age < 5.0:  # Fading back to white
            fade = (age - 2.0) / 3.0  # 0 to 1 over 3 seconds
            r = int(255 * fade + 255 * (1 - fade))
            g = int(255 * fade + 255 * (1 - fade))
            b = int(255 * fade + 0 * (1 - fade))
            return (r, g, b)
        else:
            return self.WHITE
    
    def get_cell_at_position(self, mouse_x, mouse_y):
        """Get the PID and byte index for a cell at mouse position"""
        if mouse_x < self.START_X or mouse_y < self.START_Y:
            return None
        
        # Check if click is in the data grid area
        if mouse_y >= self.START_Y + self.max_visible_rows * self.CELL_HEIGHT:
            return None
        
        col = (mouse_x - self.START_X) // self.CELL_WIDTH
        row = (mouse_y - self.START_Y) // self.CELL_HEIGHT
        
        if col < 0 or col >= 8:  # Only 8 bytes per message
            return None
        
        # Account for scrolling
        actual_row = row + self.scroll_offset
        
        if actual_row < 0 or actual_row >= len(self.pid_list):
            return None
        
        pid = self.pid_list[actual_row]
        return (pid, col)
    
    def calculate_temperature_values(self, value1, value2=None):
        """Calculate temperature using Ford's ASCII encoding discovery"""
        results = []
        
        if value2 is None:
            # Single byte analysis
            ascii_char = self.byte_to_ascii_display(value1)
            results.append((f"ASCII: '{ascii_char}' (0x{value1:02X})", 0, 0, False))
            
            # Traditional temperature guesses
            for offset in [0, 40, 50]:
                temp_f = value1 - offset
                temp_c = (temp_f - 32) * 5/9
                if 32 <= temp_f <= 120:  # Reasonable Fahrenheit range
                    results.append((f"{value1} - {offset}", temp_c, temp_f, False))
        else:
            # Two byte ASCII analysis - Ford's method!
            byte0 = value1
            byte1 = value2
            
            ascii0 = self.byte_to_ascii_display(byte0)
            ascii1 = self.byte_to_ascii_display(byte1)
            ascii_string = f"{ascii0}{ascii1}"
            
            # Show ASCII interpretation first
            results.append((f"ASCII: '{ascii_string}' (0x{byte0:02X} 0x{byte1:02X})", 0, 0, True))
            
            # Check if it's Ford's HVAC ASCII encoding (digits)
            if ascii0.isdigit() and ascii1.isdigit():
                position = int(ascii_string)
                
                # Ford HVAC temperature mapping
                if position == 60:
                    results.append((f"HVAC: LOW (ASCII position {position})", 10, 50, True))
                elif position == 90:
                    results.append((f"HVAC: HIGH (ASCII position {position})", 35, 95, True))
                elif 65 <= position <= 84:
                    temp_f = position - 5  # Position 65 = 60°F, etc.
                    temp_c = (temp_f - 32) * 5/9
                    results.append((f"HVAC: {temp_f}°F (ASCII position {position})", temp_c, temp_f, True))
                else:
                    results.append((f"ASCII Position: {position} (outside HVAC range)", 0, 0, True))
            
            # Show if bytes are printable ASCII
            if self.is_printable_ascii(byte0) and self.is_printable_ascii(byte1):
                results.append((f"Both bytes are printable ASCII!", 0, 0, True))
            elif self.is_printable_ascii(byte0) or self.is_printable_ascii(byte1):
                results.append((f"One byte is printable ASCII", 0, 0, False))
            
            # Original mathematical approach (legacy)
            temp_offset = (byte0 - 0x36) * 10 + (byte1 - 0x30)
            if 0 <= temp_offset <= 30:
                results.append((f"Math offset: {temp_offset} (legacy formula)", 0, 0, False))
            
            # Show other interpretations
            combined_be = (value1 << 8) | value2
            combined_le = (value2 << 8) | value1
            simple_add = value1 + value2
            
            for desc, test_val in [("BE", combined_be), ("LE", combined_le), ("ADD", simple_add)]:
                if desc == "ADD" and test_val < 200:
                    temp_f = test_val - 40
                    temp_c = (temp_f - 32) * 5/9
                    if 32 <= temp_f <= 120:
                        results.append((f"{desc}({test_val})-40", temp_c, temp_f, False))
        
        return results
    
    def draw_info_panel(self):
        """Draw the enhanced information panel at the bottom"""
        panel_y = self.WINDOW_HEIGHT - self.info_panel_height
        panel_rect = pygame.Rect(0, panel_y, self.WINDOW_WIDTH, self.info_panel_height)
        
        # Panel background
        pygame.draw.rect(self.screen, self.DARK_GRAY, panel_rect)
        pygame.draw.rect(self.screen, self.WHITE, panel_rect, 2)
        
        if len(self.selected_cells) > 0:
            # Display information for selected cells
            y_offset = 10
            
            # Header
            if len(self.selected_cells) == 1:
                header = "📍 Single Cell Selected - ASCII & Temperature Analysis"
            else:
                header = "📍 Two Cells Selected - Ford ASCII Decoding"
            
            text = self.big_font.render(header, True, self.WHITE)
            self.screen.blit(text, (10, panel_y + y_offset))
            y_offset += 25
            
            # Cell details with ASCII
            for i, (pid, byte_index) in enumerate(self.selected_cells):
                value = self.current_values[pid][byte_index]
                ascii_char = self.byte_to_ascii_display(value)
                change_time = self.change_times[pid][byte_index]
                
                time_str = f"{time.time() - change_time:.1f}s ago" if change_time > 0 else "Never"
                color_name = "PURPLE" if i == 0 else "CYAN"
                
                cell_info = f"Cell {i+1} ({color_name}): PID {pid}, Byte {byte_index} = 0x{value:02X} ('{ascii_char}') | Changed: {time_str}"
                text = self.info_font.render(cell_info, True, self.WHITE)
                self.screen.blit(text, (10, panel_y + y_offset))
                y_offset += 18
            
            # ASCII/Temperature analysis
            if len(self.selected_cells) == 1:
                value1 = self.current_values[self.selected_cells[0][0]][self.selected_cells[0][1]]
                results = self.calculate_temperature_values(value1)
                
                for calc, temp_c, temp_f, is_primary in results[:3]:  # Show first 3
                    color = self.GREEN if is_primary else self.YELLOW
                    text = self.small_font.render(calc, True, color)
                    self.screen.blit(text, (10, panel_y + y_offset))
                    y_offset += 16
                
            else:
                value1 = self.current_values[self.selected_cells[0][0]][self.selected_cells[0][1]]
                value2 = self.current_values[self.selected_cells[1][0]][self.selected_cells[1][1]]
                results = self.calculate_temperature_values(value1, value2)
                
                # Show results with color coding
                for calc, temp_c, temp_f, is_primary in results[:4]:  # Show first 4
                    color = self.GREEN if is_primary else self.GRAY
                    font = self.info_font if is_primary else self.small_font
                    text = font.render(calc, True, color)
                    self.screen.blit(text, (10, panel_y + y_offset))
                    y_offset += 18 if is_primary else 14
            
        else:
            # Default message
            cal_status = "CALIBRATION ON" if self.calibration_mode else "OFF"
            lines = [
                f"🖱️ ASCII/HEX EXPLORER - Mode: {self.display_mode} | Calibration: {cal_status}",
                "Press 'A' = ASCII/HEX toggle | 'T' = Temperature calibration mode",
                "🌡️ CALIBRATION: Select temp sensor cells, press T, then input LCD readings",
                "🔤 ASCII mode highlights Ford's text-based encoding schemes"
            ]
            
            for i, line in enumerate(lines):
                text = self.info_font.render(line, True, self.WHITE)
                self.screen.blit(text, (10, panel_y + 10 + i * 18))
    
    def draw_grid(self):
        """Draw the main grid"""
        self.screen.fill(self.BLACK)
        
        # Draw title and stats
        title_text = self.header_font.render(f"🌡️ F150 CAN Bus ASCII/HEX Explorer - Mode: {self.display_mode}", True, self.WHITE)
        self.screen.blit(title_text, (10, 5))
        
        # Calculate messages per second
        current_time = time.time()
        if current_time - self.last_fps_time >= 1.0:
            self.messages_per_second = self.total_messages - self.last_message_count
            self.last_message_count = self.total_messages
            self.last_fps_time = current_time
        
        stats_text = self.small_font.render(f"PIDs: {len(self.pid_list)} | Messages: {self.total_messages} | Rate: {self.messages_per_second}/s | Selected: {len(self.selected_cells)}/2", True, self.WHITE)
        self.screen.blit(stats_text, (10, 25))
        
        # Draw column headers (Byte 0-7)
        for col in range(8):
            x = self.START_X + col * self.CELL_WIDTH
            y = self.START_Y - 30
            
            # Header background
            pygame.draw.rect(self.screen, self.DARK_GRAY, (x, y, self.CELL_WIDTH, 25))
            pygame.draw.rect(self.screen, self.WHITE, (x, y, self.CELL_WIDTH, 25), 1)
            
            # Header text
            header_text = self.small_font.render(f"Byte {col}", True, self.WHITE)
            text_rect = header_text.get_rect(center=(x + self.CELL_WIDTH//2, y + 12))
            self.screen.blit(header_text, text_rect)
        
        # Calculate which PIDs to show (handle scrolling)
        start_row = self.scroll_offset
        end_row = min(start_row + self.max_visible_rows, len(self.pid_list))
        
        # Draw grid rows
        for row_idx, pid_idx in enumerate(range(start_row, end_row)):
            if pid_idx >= len(self.pid_list):
                break
                
            pid = self.pid_list[pid_idx]
            y = self.START_Y + row_idx * self.CELL_HEIGHT
            
            # Draw PID label
            pid_rect = pygame.Rect(5, y, self.PID_COLUMN_WIDTH - 10, self.CELL_HEIGHT)
            pygame.draw.rect(self.screen, self.BLUE, pid_rect)
            pygame.draw.rect(self.screen, self.WHITE, pid_rect, 1)
            
            pid_text = self.small_font.render(pid, True, self.WHITE)
            text_rect = pid_text.get_rect(center=pid_rect.center)
            self.screen.blit(pid_text, text_rect)
            
            # Draw data cells for each byte
            for col in range(8):
                x = self.START_X + col * self.CELL_WIDTH
                cell_rect = pygame.Rect(x, y, self.CELL_WIDTH, self.CELL_HEIGHT)
                
                # Get cell color based on mode, ASCII content, and changes
                cell_color = self.get_cell_color(pid, col)
                
                # Draw cell
                pygame.draw.rect(self.screen, cell_color, cell_rect)
                
                # Draw thicker border for selected cells
                cell = (pid, col)
                border_width = 3 if cell in self.selected_cells else 1
                pygame.draw.rect(self.screen, self.BLACK, cell_rect, border_width)
                
                # Draw value based on display mode
                value = self.current_values[pid][col]
                
                if self.display_mode == "HEX":
                    display_text = f"{value:02X}"
                else:  # ASCII mode
                    ascii_char = self.byte_to_ascii_display(value)
                    display_text = ascii_char
                
                value_text = self.small_font.render(display_text, True, self.BLACK)
                text_rect = value_text.get_rect(center=cell_rect.center)
                self.screen.blit(value_text, text_rect)
        
        # Draw scrollbar if needed
        if len(self.pid_list) > self.max_visible_rows:
            scrollbar_x = self.WINDOW_WIDTH - 20
            scrollbar_height = self.WINDOW_HEIGHT - self.START_Y - self.info_panel_height - 10
            
            # Scrollbar background
            pygame.draw.rect(self.screen, self.DARK_GRAY, (scrollbar_x, self.START_Y, 15, scrollbar_height))
            
            # Scrollbar thumb
            thumb_height = max(20, (self.max_visible_rows / len(self.pid_list)) * scrollbar_height)
            thumb_y = self.START_Y + (self.scroll_offset / len(self.pid_list)) * scrollbar_height
            pygame.draw.rect(self.screen, self.GRAY, (scrollbar_x, thumb_y, 15, thumb_height))
        
        # Draw display mode legend
        legend_x = self.WINDOW_WIDTH - 250
        legend_y = 50
        
        legend_items = [
            ("1st Selection", self.PURPLE),
            ("2nd Selection", self.CYAN),
            ("Recent Change", self.GREEN),
            ("ASCII Printable", self.LIGHT_GREEN) if self.display_mode == "ASCII" else ("HEX Mode", self.WHITE)
        ]
        
        for i, (label, color) in enumerate(legend_items):
            y = legend_y + i * 20
            pygame.draw.rect(self.screen, color, (legend_x, y, 15, 15))
            text = self.small_font.render(label, True, self.WHITE)
            self.screen.blit(text, (legend_x + 20, y))
        
        # Draw mode toggle hint
        toggle_text = self.small_font.render("Press 'A' to toggle ASCII/HEX", True, self.YELLOW)
        self.screen.blit(toggle_text, (legend_x, legend_y + 100))
        
        # Draw info panel
        self.draw_info_panel()
    
    def handle_events(self):
        """Handle pygame events"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
            
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    return False
                elif event.key == pygame.K_a:
                    # Toggle ASCII/HEX display mode
                    self.display_mode = "ASCII" if self.display_mode == "HEX" else "HEX"
                    print(f"🔄 Display mode: {self.display_mode}")
                elif event.key == pygame.K_t:
                    # Toggle temperature calibration mode
                    self.calibration_mode = not self.calibration_mode
                    if self.calibration_mode:
                        print("🌡️ TEMPERATURE CALIBRATION MODE ON")
                        print("📝 When CAN values change, you'll be prompted to enter actual LCD temperature")
                        print("💡 Select your temperature sensor bytes first, then press T to start calibration")
                    else:
                        print("🌡️ TEMPERATURE CALIBRATION MODE OFF")
                        if self.calibration_data:
                            print(f"\n📊 CALIBRATION DATA COLLECTED ({len(self.calibration_data)} points):")
                            print("Actual_Temp | CAN_Value | Notes")
                            print("-" * 40)
                            for temp, can_val, timestamp in self.calibration_data:
                                print(f"{temp:8.1f}°F | {can_val:8d} | {timestamp}")
                            
                            # Calculate linear regression if we have enough points
                            if len(self.calibration_data) >= 2:
                                self.calculate_temperature_formula()
                elif event.key == pygame.K_RETURN:
                    # Handle temperature input in calibration mode
                    if self.calibration_mode and len(self.selected_cells) == 2:
                        self.handle_temperature_input()
                elif event.key == pygame.K_SPACE:
                    # Toggle pause (stop serial reading)
                    pass
                elif event.key == pygame.K_UP:
                    self.scroll_offset = max(0, self.scroll_offset - 1)
                elif event.key == pygame.K_DOWN:
                    max_offset = max(0, len(self.pid_list) - self.max_visible_rows)
                    self.scroll_offset = min(max_offset, self.scroll_offset + 1)
                elif event.key == pygame.K_c:
                    # Clear selection
                    self.selected_cells = []
                    print("🗑️ Selection cleared")
            
            elif event.type == pygame.MOUSEWHEEL:
                if event.y > 0:  # Scroll up
                    self.scroll_offset = max(0, self.scroll_offset - 3)
                else:  # Scroll down
                    max_offset = max(0, len(self.pid_list) - self.max_visible_rows)
                    self.scroll_offset = min(max_offset, self.scroll_offset + 3)
            
            elif event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1:  # Left click
                    mouse_x, mouse_y = pygame.mouse.get_pos()
                    cell = self.get_cell_at_position(mouse_x, mouse_y)
                    if cell:
                        if cell in self.selected_cells:
                            # Deselect if already selected
                            self.selected_cells.remove(cell)
                            print(f"🚫 Deselected: PID {cell[0]}, Byte {cell[1]}")
                        else:
                            # Add to selection (max 2)
                            if len(self.selected_cells) < 2:
                                self.selected_cells.append(cell)
                                pid, byte_index = cell
                                value = self.current_values[pid][byte_index]
                                ascii_char = self.byte_to_ascii_display(value)
                                print(f"✅ Selected {len(self.selected_cells)}/2: PID {pid}, Byte {byte_index}, Value: 0x{value:02X} ('{ascii_char}')")
                                
                                # If we now have 2 cells selected, start logging mode
                                if len(self.selected_cells) == 2:
                                    print("\n🔬 ASCII/HEX LOGGING MODE ACTIVATED!")
                                    print("📝 Adjust controls and watch for ASCII patterns:")
                                    print("HEX      ASCII    | HEX      ASCII    | Combined | ASCII String")
                                    print("-" * 70)
                                    
                            else:
                                # Replace oldest selection
                                old_cell = self.selected_cells.pop(0)
                                self.selected_cells.append(cell)
                                pid, byte_index = cell
                                value = self.current_values[pid][byte_index]
                                ascii_char = self.byte_to_ascii_display(value)
                                print(f"🔄 Replaced selection: PID {pid}, Byte {byte_index}, Value: 0x{value:02X} ('{ascii_char}')")
        
        return True
    
    def run(self):
        """Run the visual monitor"""
        if not self.connect():
            return
        
        print("🎯 ASCII/HEX Explorer Started!")
        print("🔤 Press 'A' to toggle between ASCII and HEX display modes")
        print("🌡️ Press 'T' to toggle temperature calibration mode")
        print("🖱️ Click on cells to analyze ASCII patterns and temperature values")
        print("💡 Green cells in ASCII mode = printable ASCII characters")
        print("🔍 Perfect for finding Ford's ASCII-encoded systems!")
        print("\n📋 TEMPERATURE CALIBRATION WORKFLOW:")
        print("1. Select your temperature sensor bytes (PID 0x3C4, bytes 6&7)")
        print("2. Press 'T' to enter calibration mode")
        print("3. When values change, manually input actual LCD temperature")
        print("4. Press 'T' again to exit and see calculated formula")
        
        self.running = True
        
        # Start serial reading thread
        serial_thread = threading.Thread(target=self.read_serial_data, daemon=True)
        serial_thread.start()
        
        # Main display loop
        clock = pygame.time.Clock()
        
        try:
            while self.running:
                if not self.handle_events():
                    break
                
                self.draw_grid()
                pygame.display.flip()
                clock.tick(30)  # 30 FPS
        
        except KeyboardInterrupt:
            pass
        
        finally:
            self.running = False
            if self.serial:
                self.serial.close()
            pygame.quit()
            
            # Print final summary
            print("\n🎯 ASCII/HEX Explorer Session Complete!")
            print(f"📊 Total PIDs observed: {len(self.pid_list)}")
            print(f"📈 Total messages: {self.total_messages}")
            print("💡 Use ASCII mode to spot Ford's text-based encoding patterns!")

def main():
    parser = argparse.ArgumentParser(description="CAN Bus ASCII/HEX Explorer")
    parser.add_argument("port", help="Serial port (e.g., /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    
    args = parser.parse_args()
    
    try:
        monitor = CANVisualGrid(args.port, args.baud)
        monitor.run()
    except Exception as e:
        print(f"Error: {e}")
        print("Make sure pygame is installed: pip install pygame")

if __name__ == "__main__":
    main()
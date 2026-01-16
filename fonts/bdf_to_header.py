import sys
import re

def bdf_to_c_header(filename, array_name):
    with open(filename, 'r') as f:
        content = f.read()

    # Find all characters and their bitmaps
    # This regex looks for the ENCODING (ASCII) and the BITMAP data
    chars = re.findall(r'ENCODING (\d+).*?BITMAP\n(.*?)\nENDCHAR', content, re.DOTALL)

    print(f"// Generated from {filename}")
    print(f"unsigned char {array_name}[256][16] = {{")

    for encoding, bitmap in chars:
        ascii_val = int(encoding)
        if 0 <= ascii_val < 256:
            # Convert the hex lines into C-style hex: 0xAA, 0xBB...
            hex_values = [f"0x{line}" for line in bitmap.strip().split('\n')]
            
            # Pad if the font isn't exactly 16 rows high
            while len(hex_values) < 16:
                hex_values.append("0x00")
            
            c_line = ", ".join(hex_values)
            print(f"    [{ascii_val}] = {{ {c_line} }},")

    print("};")

if __name__ == "__main__":
    # Change 'ter-u16n.bdf' to whichever version you want
    bdf_to_c_header('terminus-font-4.49.1/ter-u16n.bdf', 'terminus_16n')

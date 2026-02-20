from PIL import Image, ImageDraw, ImageFont

# Create a blank image
img = Image.new("RGB", (1100, 700), "white")
draw = ImageDraw.Draw(img)

# Define colors
cell_color = "#c4e1ff"
bms_color = "#ffd966"
line_color = "black"
text_color = "black"
charger_color = "#b3e6b3"
load_color = "#ffcccc"

# Draw 3 series-connected groups of 4 cells in parallel (3S4P)
cell_width = 40
cell_height = 80
padding_x = 80
padding_y = 120
group_spacing = 220

for i in range(3):  # 3 groups in series
    x0 = padding_x + i * group_spacing
    for j in range(4):  # 4 parallel cells per group
        y0 = padding_y + j * (cell_height + 5)
        draw.rectangle([x0, y0, x0 + cell_width, y0 + cell_height], fill=cell_color, outline="black")
    draw.text((x0 + 5, padding_y - 30), f"Cell Group {i+1}", fill=text_color)

# Draw BMS block
bms_x = 740
bms_y = 150
bms_w = 180
bms_h = 220
draw.rectangle([bms_x, bms_y, bms_x + bms_w, bms_y + bms_h], fill=bms_color, outline="black")
draw.text((bms_x + 70, bms_y + 10), "BMS", fill=text_color)

# Draw BMS terminals with voltage labels
terminals = ["0V", "4.2V", "8.4V", "12.6V", "P+", "P-"]
for i, t in enumerate(terminals):
    ty = bms_y + 40 + i * 30
    draw.text((bms_x + 10, ty), t, fill=text_color)
    draw.line([(bms_x, ty + 8), (bms_x - 40, ty + 8)], fill=line_color, width=2)

# Connect battery groups to BMS voltage terminals
# Map: 
# 0V   -> bottom of first group (lowest voltage)
# 4.2V -> top of first group
# 8.4V -> top of second group
# 12.6V -> top of third group

# Bottom of first group (0V)
draw.line([
    (padding_x, padding_y + 3 * (cell_height + 5) + cell_height // 2),
    (bms_x - 40, bms_y + 40 + 0 * 30 + 8)
], fill=line_color, width=2)

# Top of first group (4.2V)
draw.line([
    (padding_x + cell_width, padding_y + cell_height // 2),
    (bms_x - 40, bms_y + 40 + 1 * 30 + 8)
], fill=line_color, width=2)

# Top of second group (8.4V)
draw.line([
    (padding_x + group_spacing + cell_width, padding_y + cell_height // 2),
    (bms_x - 40, bms_y + 40 + 2 * 30 + 8)
], fill=line_color, width=2)

# Top of third group (12.6V)
draw.line([
    (padding_x + 2 * group_spacing + cell_width, padding_y + cell_height // 2),
    (bms_x - 40, bms_y + 40 + 3 * 30 + 8)
], fill=line_color, width=2)

# Draw charger block
charger_x = 740
charger_y = bms_y + bms_h + 40
charger_w = 150
charger_h = 100
draw.rectangle([charger_x, charger_y, charger_x + charger_w, charger_y + charger_h], fill=charger_color, outline="black")
draw.text((charger_x + 25, charger_y + 40), "Charger\n12.6V Li-ion", fill=text_color)

# Draw load block (e.g. Peltier coolers)
load_x = bms_x - 140
load_y = charger_y
load_w = 140
load_h = 100
draw.rectangle([load_x, load_y, load_x + load_w, load_y + load_h], fill=load_color, outline="black")
draw.text((load_x + 10, load_y + 40), "Load\n(Peltier Coolers)", fill=text_color)

# Coordinates for P+ and P- terminals on BMS
p_plus_y = bms_y + 40 + 4 * 30 + 8
p_minus_y = bms_y + 40 + 5 * 30 + 8
split_x = bms_x + bms_w + 40

# Draw split junction dots and lines for P+
draw.ellipse((split_x - 5, p_plus_y - 5, split_x + 5, p_plus_y + 5), fill="black")
draw.line([(bms_x + bms_w, p_plus_y), (split_x, p_plus_y)], fill="red", width=3)
draw.line([(split_x, p_plus_y), (charger_x, charger_y + 30)], fill="red", width=3)
draw.line([(split_x, p_plus_y), (load_x + load_w, load_y + 30)], fill="red", width=3)
draw.text((split_x + 10, p_plus_y - 15), "Split +", fill="red")

# Draw split junction dots and lines for P-
draw.ellipse((split_x - 5, p_minus_y - 5, split_x + 5, p_minus_y + 5), fill="black")
draw.line([(bms_x + bms_w, p_minus_y), (split_x, p_minus_y)], fill="blue", width=3)
draw.line([(split_x, p_minus_y), (charger_x, charger_y + charger_h - 30)], fill="blue", width=3)
draw.line([(split_x, p_minus_y), (load_x + load_w, load_y + load_h - 30)], fill="blue", width=3)
draw.text((split_x + 10, p_minus_y + 5), "Split -", fill="blue")

# Add labels near charger and load wires
draw.text((charger_x + 10, charger_y + 10), "Charger +", fill="red")
draw.text((charger_x + 10, charger_y + charger_h - 50), "Charger -", fill="blue")
draw.text((load_x - 80, load_y + 10), "Load +", fill="red")
draw.text((load_x - 80, load_y + load_h - 50), "Load -", fill="blue")

# Show image
img.show()

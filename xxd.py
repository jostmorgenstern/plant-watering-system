import os

for filename in os.listdir('static'):
    path = os.path.join('static', filename)
    basename = os.path.splitext(filename)[0]
    print(f'static const char {basename}[] = {{', end='')
    with open(path, 'rb') as f:
        print(", ".join("0x%02x" % byte for byte in f.read()), end='')
    print(", 0x00};\n")
        

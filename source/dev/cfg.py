from sys import argv
from itertools import zip_longest

PIXELS_PER_CHUNK = 64
BYTES_PER_PIXEL = 3
MAX_COLORS = 8

def encode(itr, file):
    
    # The graphic data parser assigns special meaning to the color 
    # magenta. This color represents transparency
    palette = [(0xFF, 0x00, 0xFF),]
    data = []
    for chunk in itr:
        chunk = tuple(zip(* [iter(chunk)]*BYTES_PER_PIXEL))
        head = 0
        tail = PIXELS_PER_CHUNK-1
        while head <= tail:
            pixelTuple = chunk[head]
            try:
                paletteIndex = palette.index(pixelTuple)
            except ValueError:
                paletteIndex = len(palette)
                if paletteIndex >= MAX_COLORS:
                    print('Could not encode the image due to excessive '
                        'colours.')
                    return
                palette.append(pixelTuple)
            end = tail
            while chunk[tail] == pixelTuple:
                tail -= 1
                if head == tail:
                    break
            tailLength = end - tail
            start = head
            while chunk[head] == pixelTuple and head <= tail:
                head += 1
            headLength = head - start
            
            entry = paletteIndex << 5
            if tailLength == 0 and headLength < 16:
                entry |= headLength
                datumSize = 1
            else:
                entry |= 0x10
                entry <<= 8
                entry |= headLength << 6
                entry |= tailLength
                datumSize = 2
            data.append(entry.to_bytes(datumSize, byteorder='big', 
                signed=False))
    
    # Remove magenta
    palette.pop(0)
    
    file.write(len(palette).to_bytes(1, signed=False))
    
    for i, t in enumerate(palette):
        if t == None:
            break
        pixel = (t[0]<<16 | t[1]<<8 | t[2])
        byteTriplet = pixel.to_bytes(BYTES_PER_PIXEL, byteorder='big',
            signed=False)
        file.write(byteTriplet)
    for pixel in data:
        file.write(pixel)
        

if len(argv) == 2:
    try:
        with open(argv[1], 'rb') as i:
            i.seek(0x0A)
            offset = int.from_bytes(i.read(4), 'little')
            i.seek(offset)
            data = i.read()
        with open(argv[1].split('.')[0] + '.cfg', 'wb') as o:
            encode(zip_longest(* [iter(data)]
                *BYTES_PER_PIXEL*PIXELS_PER_CHUNK), o)
    except FileNotFoundError as e:
        print(f'Could not find file "{argv[1]}". Script terminated.')
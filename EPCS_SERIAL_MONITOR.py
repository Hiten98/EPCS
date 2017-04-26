import serial
import unicodedata
comPort = 'COM6'

teensy = serial.Serial(comPort,9600,timeout=.9)

while True:
    data = input("Enter data to send\n")
    print(type(data))
    b = None
    b = bytes(map(ord,data))
    while data == '':
        data = input("Please enter something\n")
        b = bytes(map(ord,data))
    teensy.write(b)
    data = None
    toComp = ('Please','enter','something')
    while data is None or data.startswith('Please enter something') or flag:
        print('Waiting for response...')
        data = teensy.read(100)
        data = str(data,'utf-8')
        flag = any(data.find(s)>=0 for s in toComp)
        #print(data)
        #unicodedata.normalize('NFKD',data).encode('utf-8',data)
    if data:
        print(data)

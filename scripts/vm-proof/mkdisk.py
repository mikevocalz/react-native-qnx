import struct,sys,os
out,boot,sysp,data=sys.argv[1:5]
parts=[(177,boot),(178,sysp),(179,data)]
START=2048
ents=[];lba=START;layout=[]
for t,f in parts:
    n=(os.path.getsize(f)+511)//512
    layout.append((t,f,lba,n)); ents.append(struct.pack('<B3sB3sII',0x80 if t==177 else 0,b'\xfe\xff\xff',t,b'\xfe\xff\xff',lba,n)); lba+=(n+2047)//2048*2048
mbr=bytearray(512); mbr[446:446+48]=b''.join(ents); mbr[510:512]=b'\x55\xaa'
with open(out,'wb') as o:
    o.write(mbr)
    for t,f,l,n in layout:
        o.seek(l*512); o.write(open(f,'rb').read())
    o.truncate(lba*512)
print(layout)

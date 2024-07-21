Header: shoter packet length and add sequence number. 
Type(1 byte)
packet number(8 bytes)
priority(1 byte)
offset(4 bytes)
acknowledge sequence(8 bytes)
acknowledge time(1 byte)
difference(1 byte)
pkt_length(2 bytes)

Add partial send_mmsg
others follows v2.1
no longer use map to store send_buf
use deque store it again
update: 
merge mmsghdr to connection

Target:
fix receive_buf.h
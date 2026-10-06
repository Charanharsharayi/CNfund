import pyshark

capture = pyshark.FileCapture("traffic.pcapng")

count = 0

for packet in capture:

    count += 1

    print("Packet:", count)

    if hasattr(packet, "ip"):
        print("Source IP:", packet.ip.src)
        print("Destination IP:", packet.ip.dst)

    print("Protocol:", packet.highest_layer)
    print("----------------------")

capture.close()

print("Total packets:", count)
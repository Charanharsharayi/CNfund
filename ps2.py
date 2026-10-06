import pyshark

capture = pyshark.FileCapture("traffic.pcapng")

total = 0
tcp = 0
udp = 0
icmp = 0

sizes = []

for packet in capture:

    total += 1

    if hasattr(packet, "length"):
        sizes.append(int(packet.length))

    if hasattr(packet, "tcp"):
        tcp += 1

    if hasattr(packet, "udp"):
        udp += 1

    if hasattr(packet, "icmp"):
        icmp += 1

capture.close()

print("Total packets:", total)
print("TCP packets:", tcp)
print("UDP packets:", udp)
print("ICMP packets:", icmp)

if sizes:
    print("Average packet size:", sum(sizes) / len(sizes))
    print("Minimum packet size:", min(sizes))
    print("Maximum packet size:", max(sizes))
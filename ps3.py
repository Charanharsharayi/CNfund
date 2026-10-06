import pyshark

capture = pyshark.FileCapture("traffic.pcapng")

for packet in capture:

    protocol = packet.highest_layer

    if protocol == "TCP":
        traffic = "TCP Traffic"

    elif protocol == "UDP":
        traffic = "UDP Traffic"

    elif protocol == "ICMP":
        traffic = "ICMP Traffic"

    else:
        traffic = "Other Traffic"

    print(protocol, "->", traffic)

capture.close()
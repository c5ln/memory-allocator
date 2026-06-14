import matplotlib.pyplot as plt

def align16(n):
    return (n + 15) & ~15


sizes = list(range(1, 257))

usage_4 = [align16(size + 4) for size in sizes]
usage_8 = [align16(size + 8) for size in sizes]

plt.figure(figsize=(12, 6))
plt.plot(sizes, usage_4, label="4-byte Header")
plt.plot(sizes, usage_8, label="8-byte Header")

plt.xlabel("Requested Size (bytes)")
plt.ylabel("Actual Allocated Size (bytes)")
plt.title("Actual Allocation Size with 16-byte Alignment")
plt.legend()
plt.grid(True)

plt.show()
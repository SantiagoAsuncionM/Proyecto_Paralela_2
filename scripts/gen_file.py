import random
random.seed(42)
vocab = [f"palabra{i}" for i in range(20000)]
weights = [1 / (i + 1) for i in range(20000)]
target = 300 * 1024 * 1024
written = 0
with open("corpus_300mb.txt", "w") as f:
    while written < target:
        line = " ".join(random.choices(vocab, weights, k=12)) + "\n"
        f.write(line)


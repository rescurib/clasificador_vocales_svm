"""
Load MFCC datasets (mfcc_*.npy), perform PCA to 2D, and plot the result.
Labels are inferred from filenames (e.g. mfcc_a.npy → label 'a').
"""

import sys
import glob
import numpy as np
import matplotlib.pyplot as plt
from sklearn.decomposition import PCA


def main():
    # Find all mfcc_*.npy files (excluding *_files.npy)
    npy_files = sorted([
        f for f in glob.glob("mfcc_*.npy")
        if not f.endswith("_files.npy")
    ])
    if not npy_files:
        print("No mfcc_*.npy files found in current folder.")
        sys.exit(1)

    all_X = []
    all_labels = []

    for data_file in npy_files:
        # Derive label directly from filename: mfcc_a.npy → 'a'
        label = data_file.replace("mfcc_", "").replace(".npy", "")
        X = np.load(data_file)
        all_X.append(X)
        all_labels.extend([label] * len(X))
        print(f"Loaded {X.shape[0]} samples for label '{label}'.")

    X_combined = np.vstack(all_X)
    labels_combined = np.array(all_labels)
    print(f"Total samples: {X_combined.shape[0]}, features: {X_combined.shape[1]}")

    # PCA projection
    pca = PCA(n_components=2)
    X_pca = pca.fit_transform(X_combined)

    plt.figure(figsize=(8, 6))
    for lbl in sorted(set(labels_combined)):
        idx = labels_combined == lbl
        plt.scatter(X_pca[idx, 0], X_pca[idx, 1], label=lbl, alpha=0.7)

    plt.title("Proyección PCA de características MFCC")
    plt.xlabel("Componente Principal 1")
    plt.ylabel("Componente Principal 2")
    plt.legend(title="Etiqueta", loc="best")
    plt.grid(True)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
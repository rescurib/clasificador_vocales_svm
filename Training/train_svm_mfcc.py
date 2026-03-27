"""
Entrena un modelo SVM usando scikit-learn con datos MFCC etiquetados.
Los archivos .npy deben estar en el directorio actual y deben tener el formato mfcc_<label>.npy

Uso:
    python train_svm_mfcc.py

Para multiclase con CMSIS-DSP, entrenamos N clasificadores binarios LinearSVC independientes
explícitamente (One-vs-Rest), uno por clase. Cada uno se exporta como su propio conjunto de arrays de C
listos para inicializar una estructura arm_svm_linear_instance_f32 independiente.
"""

import numpy as np
import glob
import sys
from sklearn.svm import LinearSVC
from sklearn.multiclass import OneVsRestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, accuracy_score

def format_float_array(arr, name, indent=2):
    """Formatea un array de numpy como un literal de array float32_t de C."""
    spaces = " " * indent
    items = [f"{v:.8f}f" for v in arr.flatten()]
    lines = []
    line = spaces
    for i, item in enumerate(items):
        addition = item + (", " if i < len(items) - 1 else "")
        if len(line) + len(addition) > 80:
            lines.append(line.rstrip())
            line = spaces + addition
        else:
            line += addition
    if line.strip():
        lines.append(line.rstrip())
    return f"const float32_t {name}[] = {{\n" + "\n".join(lines) + "\n};"


def format_int_array(arr, name, indent=2):
    """Formatea un array de enteros como un literal de array int32_t de C."""
    spaces = " " * indent
    body = spaces + ", ".join(str(int(v)) for v in arr)
    return f"const int32_t {name}[] = {{\n{body}\n}};"


def export_h_file(ovr_clf, label_to_int, n_features, output_path="svm_params.h"):
    """
    Exporta los parámetros de los clasificadores binarios LinearSVC One-vs-Rest como un encabezado de C.

    OneVsRestClassifier expone .estimators_, una lista de N LinearSVC ajustados,
    uno por clase. Cada LinearSVC tiene:
      .coef_        forma: (1, n_features)  — el vector normal del hiperplano
      .intercept_   forma: (1,)             — el término de sesgo (bias)

    arm_svm_linear de CMSIS-DSP usa la formulación del producto escalar:
      decisión = dot(dualCoefficients * supportVectors) + intercept
    que es equivalente a:
      decisión = dot(w, x) + b
    por lo que codificamos el vector de pesos w como un único "vector de soporte" con
    coeficiente dual = 1.0, lo que da el mismo resultado.
    """
    unique_labels = sorted(label_to_int.keys())
    n_classes     = len(unique_labels)

    guard = "SVM_PARAMS_H"
    lines = []
    lines.append("/* Generado automáticamente por train_svm_mfcc.py — no editar manualmente */")
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append('#include "arm_math.h"')
    lines.append("")
    lines.append(f"#define VECTOR_DIMENSION    {n_features}")
    lines.append(f"#define NB_CLASSIFIERS      {n_classes}")
    lines.append("")
    lines.append("/*")
    lines.append("  Mapeo de clases (etiqueta -> ID entero):")
    for label, idx in label_to_int.items():
        lines.append(f"    '{label}' -> {idx}")
    lines.append("  Cada clasificador K genera:")
    lines.append("    classes[0] = K  -> la entrada pertenece a la clase K")
    lines.append("    classes[1] = -1 -> la entrada pertenece al 'resto'")
    lines.append("  Ejecute todos los clasificadores y elija el que prediga classes[0].")
    lines.append("*/")
    lines.append("")

    for k, label in enumerate(unique_labels):
        id_k      = label_to_int[label]
        suffix    = f"{label}_vs_rest"
        estimator = ovr_clf.estimators_[k]  # LinearSVC ajustado para esta clase

        # Decisión de LinearSVC: dot(coef_, x) + intercept_
        # Representamos esto como un único vector de soporte (el vector de pesos)
        # con un coeficiente dual de 1.0, lo cual es matemáticamente idéntico.
        w             = estimator.coef_.flatten()        # forma: (n_features,)
        intercept_val = float(estimator.intercept_[0])

        lines.append(f"/* --- Clasificador {k}: '{label}' (id={id_k}) vs resto --- */")
        lines.append(f"/*     decisión = dot(w, x) + {intercept_val:.8f}          */")
        lines.append(f"#define NB_SV_{suffix.upper()}      1")
        lines.append(f"#define INTERCEPT_{suffix.upper()}  {intercept_val:.8f}f")
        lines.append("")
        lines.append("/* Coeficiente dual: fijado en 1.0 (w se codifica como el vector de soporte) */")
        lines.append(format_float_array(np.array([1.0]), f"dualCoefficients_{suffix}"))
        lines.append("")
        lines.append("/* Vector de soporte = vector de pesos w del hiperplano lineal */")
        lines.append(format_float_array(w, f"supportVectors_{suffix}"))
        lines.append("")
        lines.append(format_int_array([-1, id_k], f"classes_{suffix}"))  # IDs de clase: [clase objetivo, centinela de resto]
        lines.append("")

    lines.append(f"#endif /* {guard} */")
    lines.append("")

    with open(output_path, "w") as f:
        f.write("\n".join(lines))

    print(f"Header written to: {output_path}")
    for k, label in enumerate(unique_labels):
        print(f"  Classifier {k}: '{label}' vs rest")


def main():
    npy_files = sorted([
        f for f in glob.glob("mfcc_*.npy")
        if not f.endswith("_files.npy")
    ])
    if not npy_files:
        print("No mfcc_*.npy files found in current folder.")
        sys.exit(1)

    all_X      = []
    all_labels = []

    for data_file in npy_files:
        label = data_file.replace("mfcc_", "").replace(".npy", "")
        X = np.load(data_file)
        all_X.append(X)
        all_labels.extend([label] * len(X))
        print(f"Loaded {X.shape[0]} samples for label '{label}'.")

    X_combined = np.vstack(all_X)
    y_combined = np.array(all_labels)
    print(f"Total samples: {X_combined.shape[0]}, features: {X_combined.shape[1]}")

    unique_labels = sorted(set(all_labels))
    label_to_int  = {label: idx for idx, label in enumerate(unique_labels)}
    print("Label mapping:", label_to_int)

    X_train, X_test, y_train, y_test = train_test_split(
        X_combined, y_combined, test_size=0.2, random_state=42, stratify=y_combined
    )

    print("Training OvR linear SVM classifiers...")
    ovr_clf = OneVsRestClassifier(LinearSVC(random_state=42), n_jobs=-1)
    ovr_clf.fit(X_train, y_train)

    y_pred = ovr_clf.predict(X_test)
    print(f"Test accuracy: {accuracy_score(y_test, y_pred):.4f}")
    print(classification_report(y_test, y_pred))

    n_features = X_combined.shape[1]
    print(f"\n{len(unique_labels)} classes -> {len(unique_labels)} OvR binary classifiers to export")
    export_h_file(ovr_clf, label_to_int, n_features, output_path="svm_params.h")


if __name__ == "__main__":
    main()
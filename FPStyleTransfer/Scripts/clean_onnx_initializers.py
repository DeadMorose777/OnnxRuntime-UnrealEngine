import argparse
from pathlib import Path

import onnx


def clean_model(source_path: Path, destination_path: Path) -> bool:
    model = onnx.load(str(source_path))

    initializer_names = {initializer.name for initializer in model.graph.initializer}
    if not initializer_names:
        return False

    original_input_count = len(model.graph.input)
    filtered_inputs = [tensor for tensor in model.graph.input if tensor.name not in initializer_names]

    if len(filtered_inputs) == original_input_count:
        # Nothing to strip.
        return False

    # Replace the repeated field contents.
    del model.graph.input[:]
    model.graph.input.extend(filtered_inputs)

    destination_path.parent.mkdir(parents=True, exist_ok=True)
    onnx.save(model, str(destination_path))
    return True


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Remove initializer tensors from the graph input list so that ONNX Runtime treats them as constants.",
    )
    parser.add_argument(
        "inputs",
        nargs="+",
        help="Paths to .onnx files to clean.",
    )
    parser.add_argument(
        "--suffix",
        default=".cleaned.onnx",
        help="Suffix to append to cleaned models (default: %(default)s).",
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="Overwrite the original file instead of creating a copy.",
    )
    args = parser.parse_args()

    for input_path in args.inputs:
        source = Path(input_path).resolve()
        if not source.is_file():
            print(f"[WARN] Skip '{source}': not a file.")
            continue

        if args.in_place:
            destination = source
        else:
            destination = source.with_suffix("")
            destination = destination.with_name(destination.name + args.suffix)

        changed = clean_model(source, destination)
        if changed:
            print(f"[OK] Cleaned '{source.name}' -> '{destination.name}'")
        else:
            if args.in_place:
                print(f"[SKIP] '{source.name}' already clean (no initializer inputs).")
            else:
                print(f"[SKIP] '{source.name}' already clean. No file emitted.")


if __name__ == "__main__":
    main()

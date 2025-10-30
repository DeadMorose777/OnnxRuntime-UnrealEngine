# FPStyleTransfer (Unreal Engine 5.5)

Realtime neural style transfer sample updated to Unreal Engine 5.5 and the Neural Network Engine (NNE) Render Dependency Graph (RDG) runtime. The project keeps all tensors on the GPU – camera input is encoded with a compute shader, inference runs through `INNERuntimeRDG::EnqueueRDG`, and the stylised frame is decoded and composited as a post-tonemap pass.

## Highlights
- Works out of the box on UE 5.5 with Direct3D 12 (fallback to D3D11 for debugging only).
- Uses the NNE `NNERuntimeRDG` or `NNERuntimeORTDml` backends – no CPU staging buffers.
- Custom RDG pipeline (`StyleTransferShaders.usf`) to encode the scene into model input tensors and decode the inference output back to an HDR buffer.
- Sample Blueprint (`Content/FirstPerson/Blueprints/StyleTransferConfig`) that exposes style switching and hotkeys.
- Utility script to tidy exported ONNX models before import (`Scripts/clean_onnx_initializers.py`).

## Requirements
- Windows 10/11 with a D3D12 capable GPU.
- Unreal Engine **5.5** (tested with the launcher build).
- Visual Studio 2022 with the **Game development with C++** workload if you intend to build from source.
- Python 3.8+ with `onnx` installed (`pip install onnx`) to run the cleaning script.

## Getting Started

### 1. Clone
```bash
git clone https://github.com/DeadMorose777/OnnxRuntime-UnrealEngine.git
cd OnnxRuntime-UnrealEngine/FPStyleTransfer
```

### 2. Enable plugins
Inside the Unreal Editor open **Edit ▸ Plugins** and make sure the following are enabled:
- **Neural Network Engine**
- **Neural Rendering** (ships with UE; provides useful examples)

Restart the editor when prompted.

### 3. Build
You can build either from Visual Studio or the command line:

```powershell
"C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat" ^
    FPStyleTransferEditor Win64 Development -Project="%CD%\FPStyleTransfer.uproject" -WaitMutex
```

Once the Editor target compiles, double-click `FPStyleTransfer.uproject` to open the sample map (`Content/FirstPerson/Maps/FirstPersonMap`).

## Preparing Neural Style Models

1. Export your model to ONNX. The sample network expects NCHW input shaped `1 x 3 x 224 x 224`.
2. Remove initialiser tensors from the graph inputs – this keeps weights on the GPU and avoids const-folding issues:
   ```bash
   cd Scripts
   py clean_onnx_initializers.py ..\FPStyleTransfer\Content\Models\your_model.onnx
   ```
   The script emits `your_model.cleaned.onnx` alongside the original.
3. Import the cleaned ONNX file into Unreal:
   - In the Content Browser choose **Add ▸ Import to…** and pick the `.cleaned.onnx`.
   - When prompted, create an **NNE Model Data** asset (leave precision at FP32).
   - Update the **Runtime** property of the asset if you know you will target a specific backend (e.g. `NNERuntimeORTDml` on Windows).
4. (Optional) Move the asset into `Content/Models` to mirror the existing samples.

## Driving the Effect

### Blueprint sample
`Content/FirstPerson/Blueprints/StyleTransferConfig` demonstrates how to:

1. Call `UStyleTransferBlueprintLibrary::SetStyle(ModelData, RuntimeName)` on Begin Play to choose the default style.
2. Bind hotkeys to swap between different `UNNEModelData` assets.
3. Disable the pass by calling `SetStyle` with a `None` reference (this also forces `r.RealtimeStyleTransfer.Enable = 0`).

Feel free to duplicate the blueprint and customise the bindings or UI.

### Console and logging
- Enable or disable the pass manually: `r.RealtimeStyleTransfer.Enable 1` / `0`.
- Switch log detail while debugging:
  ```text
  Log LogRealtimeStyleTransfer VeryVerbose
  Log LogStyleTransferNNE Verbose
  ```

### Runtime selection
`SetStyle` accepts an optional runtime name. Pass `NNERuntimeRDGHlsl` to force the HLSL backend or `NNERuntimeORTDml` to use the DirectML implementation. When no name is provided, the runtime configured on the `UNNEModelData` asset is used.

## Project Structure

| Path | Purpose |
|------|---------|
| `Source/FPStyleTransfer/RealtimeStyleTransferViewExtension.*` | Registers the RDG post-processing extension and drives encode/inference/decode. |
| `Source/FPStyleTransfer/StyleTransferShaders.*` & `Shaders/StyleTransfer.usf` | Custom compute shaders that convert between render targets and tensors. |
| `Source/FPStyleTransfer/MyNeuralNetwork.*` | Thin wrapper that creates an `IModelInstanceRDG` and stores tensor metadata on the game thread. |
| `Source/FPStyleTransfer/StyleTransferBlueprintLibrary.*` | Exposes `SetStyle` to Blueprints and the console. |
| `Scripts/clean_onnx_initializers.py` | Helper for sanitising exported ONNX graphs. |
| `Content/Models/*.cleaned.onnx` | Cleaned models used by the sample. |

## Troubleshooting

- **No visual change** – ensure the runtime console variable is enabled (`r.RealtimeStyleTransfer.Enable 1`) and that a model is assigned via `SetStyle`. Check logs for the “Style transfer enabled…” message.
- **Enqueue failures** – the log reports the RDG status code. Verify the runtime being requested is available (Plugins window) and that you are running with D3D12.
- **Model import warnings** – run the cleaning script. NNE expects initialiser tensors to live in `graph.initializer`, not in the input list.
- **Performance** – the sample runs at 224×224. Increase the tensor resolution in `RealtimeStyleTransferViewExtension.cpp` to trade speed for quality.

## Links & Further Reading
- [Neural Post Processing with Unreal Engine NNE](https://dev.epicgames.com/community/learning/tutorials/7dr8/unreal-engine-nne-neural-post-processing)
- Epic’s `NeuralRendering` plugin (located in `Engine/Plugins/Experimental/NeuralRendering/`) contains additional examples that inspired this project.

## License
The original FPStyleTransfer sample is distributed by Microsoft. Refer to the repository licence file for details.

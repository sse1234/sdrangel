<h1>CW Demodulator (ML) Plugin</h1>

<h2>Introduction</h2>

This channel plugin decodes CW (Morse) transmissions with the
[morse-pro](https://github.com/scp93ch/morse-pro) machine-learning streaming
decoder: a causal Conformer/CTC model running via onnxruntime, fed by an
envelope feature extractor. One decoder pipeline runs per channel instance,
decoding incrementally with ~640 ms chunk latency.

The channel does standard CW reception internally: the selected frequency is
mixed to baseband, band-filtered, resampled to 8 kHz and shifted to a 600 Hz
pitch before entering the feature extractor.

<h2>Build</h2>

This plugin is not built unless `CWML_DIR` points at a morse-pro checkout:

```
cd <morse-pro>/cpp && ./fetch_onnxruntime.sh
cmake <sdrangel> ... -DCWML_DIR=<morse-pro>
```

The default model is loaded from `<morse-pro>/models/cw-stream-v1`. A
different model directory (containing `model-streaming.onnx`,
`model-streaming.onnx.data` and `model-streaming.json`) can be selected in
the GUI.

<h2>Interface</h2>

<h3>1: Frequency shift</h3>

Shift of the channel center frequency from the device center frequency.
Tune this onto the CW carrier.

<h3>2: BW</h3>

Channel filter bandwidth (default 500 Hz).

<h3>3: Model...</h3>

Select the directory containing the streaming model files.

<h3>4: Clear</h3>

Clears the decoded text window.

<h3>5: Model status</h3>

Shows whether the model is loaded, or the load/inference error otherwise.

<h3>6: Decoded text</h3>

Incrementally decoded text. Expect roughly one chunk (640 ms) of latency
plus the model's emission latency.

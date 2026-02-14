# @yume-chan/h264bsd

A WebAssembly H.264 Decoder.

A recompilation of https://github.com/oneam/h264bsd and https://github.com/udevbe/tinyh264, with modern ESM wrapper and TypeScript definition.

|              | TinyH264  | This library |
| ------------ | --------- | ------------ |
| Size         | 184KB     | 176KB        |
| CPU Usage \* | 300%~350% | 50%~60%      |

\* Decoding a 3200x1440@90FPS video. I did nothing, I have no idea where the performance diff come from

## Profile

Only Baseline profile is supported.

## Usage

```ts
import YUVCanvas from "yuv-canvas";

import type { Decoder } from "@yume-chan/h264bsd";
import initialize from "@yume-chan/h264bsd";

declare const canvas: HTMLCanvasElement | OffscreenCanvas;
declare const webGl = true;
const Renderer: YUVCanvas = YUVCanvas.attach(canvas, { webGL: webGl });

const Module = await initialize();
const Decoder: Decoder = new Module.Decoder();

export function decode(data: Uint8Array) {
  const result = Decoder.decode(data, 0, 1);

  if (result.picture) {
    const width = result.picture.info.picWidth;
    const height = result.picture.info.picHeight;

    const chromaWidth = width / 2;
    const chromaHeight = height / 2;

    const uOffset = width * height;
    const vOffset = uOffset + chromaWidth * chromaHeight;

    Renderer.drawFrame({
      format: {
        width,
        height,
        chromaWidth,
        chromaHeight,
        cropLeft: result.picture.info.cropParams.cropLeftOffset,
        cropWidth: result.picture.info.cropParams.cropOutWidth,
        cropTop: result.picture.info.cropParams.cropTopOffset,
        cropHeight: result.picture.info.cropParams.cropOutHeight,
        displayWidth: result.picture.info.cropParams.cropOutWidth,
        displayHeight: result.picture.info.cropParams.cropOutHeight,
      },
      y: {
        bytes: result.picture.bytes.subarray(0, uOffset),
        stride: width,
      },
      u: {
        bytes: result.picture.bytes.subarray(uOffset, vOffset),
        stride: chromaWidth,
      },
      v: {
        bytes: result.picture.bytes.subarray(vOffset),
        stride: chromaWidth,
      },
    });
  }
}

export function dispose() {
    Decoder.delete();
}
```

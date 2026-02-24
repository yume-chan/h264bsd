# @yume-chan/h264bsd

A WebAssembly H.264 Decoder.

A recompilation of https://github.com/oneam/h264bsd and https://github.com/udevbe/tinyh264, with modern ESM wrapper and TypeScript definition.

|              | TinyH264  | This library |
| ------------ | --------- | ------------ |
| Size         | 184KB     | 165KB        |
| CPU Usage \* | 300%~350% | 50%~60%      |

\* Decoding a 3200x1440@90FPS video. I did nothing, I have no idea where the performance diff come from

## Profile

Only Baseline profile is supported.

## Usage

```ts
import YUVCanvas from "yuv-canvas";

import type { Decoder, Picture } from "@yume-chan/h264bsd";
import initialize from "@yume-chan/h264bsd";

declare const canvas: HTMLCanvasElement | OffscreenCanvas;
declare const webGl = true;
const Renderer: YUVCanvas = YUVCanvas.attach(canvas, { webGL: webGl });

function drawPicture(picture: Picture) {
  const width = picture.info.picWidth;
  const height = picture.info.picHeight;

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
      cropLeft: picture.info.cropParams.cropLeftOffset,
      cropWidth: picture.info.cropParams.cropOutWidth,
      cropTop: picture.info.cropParams.cropTopOffset,
      cropHeight: picture.info.cropParams.cropOutHeight,
      displayWidth: picture.info.cropParams.cropOutWidth,
      displayHeight: picture.info.cropParams.cropOutHeight,
    },
    y: {
      bytes: picture.bytes.subarray(0, uOffset),
      stride: width,
    },
    u: {
      bytes: picture.bytes.subarray(uOffset, vOffset),
      stride: chromaWidth,
    },
    v: {
      bytes: picture.bytes.subarray(vOffset),
      stride: chromaWidth,
    },
  });
}

const Module = await initialize();
const Decoder: Decoder = new Module.Decoder();

let picId = 0;
export function decode(data: Uint8Array) {
  // Each `data` must be one or multiple complete NAL units
  const result = Decoder.decode(data, picId);
  picId += 1;

  // `picture`s reference internal memory
  // They are valid until next `decode` call
  // They don't need to be released
  if (result.picture) {
    // If frame reordering is enabled,
    // `result.picture.picId` might not equal to current `picId` in `decode` call
    console.log(result.picture.picId);

    drawPicture(result.picture);

    // If frame reordering is enabled,
    // one `decode` call might return multiple pictures
    for (let i = 0; i < result.extraPictureCount; i += 1) {
      drawPicture(decoder.getNextPicture());
    }
  }
}

export function dispose() {
  Decoder.delete();
}
```

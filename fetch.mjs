import { writeFile, mkdir, access } from "node:fs/promises";
import { dirname, resolve } from "node:path";

const sourceFiles = [
  "source/h264bsd_byte_stream.c",
  "source/h264bsd_byte_stream.h",
  "source/h264bsd_cavlc.c",
  "source/h264bsd_cavlc.h",
  "source/h264bsd_cfg.h",
  "source/h264bsd_conceal.c",
  "source/h264bsd_conceal.h",
  "source/h264bsd_container.h",
  "source/h264bsd_deblocking.c",
  "source/h264bsd_deblocking.h",
  "source/h264bsd_decoder.c",
  "source/h264bsd_decoder.h",
  "source/h264bsd_dpb.c",
  "source/h264bsd_dpb.h",
  "source/h264bsd_image.c",
  "source/h264bsd_image.h",
  "source/h264bsd_inter_prediction.c",
  "source/h264bsd_inter_prediction.h",
  "source/h264bsd_intra_prediction.c",
  "source/h264bsd_intra_prediction.h",
  "source/h264bsd_macroblock_layer.c",
  "source/h264bsd_macroblock_layer.h",
  "source/h264bsd_nal_unit.c",
  "source/h264bsd_nal_unit.h",
  "source/h264bsd_neighbour.c",
  "source/h264bsd_neighbour.h",
  "source/h264bsd_pic_order_cnt.c",
  "source/h264bsd_pic_order_cnt.h",
  "source/h264bsd_pic_param_set.c",
  "source/h264bsd_pic_param_set.h",
  "source/h264bsd_reconstruct.c",
  "source/h264bsd_reconstruct.h",
  "source/h264bsd_sei.c",
  "source/h264bsd_sei.h",
  "source/h264bsd_seq_param_set.c",
  "source/h264bsd_seq_param_set.h",
  "source/h264bsd_slice_data.c",
  "source/h264bsd_slice_data.h",
  "source/h264bsd_slice_group_map.c",
  "source/h264bsd_slice_group_map.h",
  "source/h264bsd_slice_header.c",
  "source/h264bsd_slice_header.h",
  "source/h264bsd_storage.c",
  "source/h264bsd_storage.h",
  "source/h264bsd_stream.c",
  "source/h264bsd_stream.h",
  "source/h264bsd_transform.c",
  "source/h264bsd_transform.h",
  "source/h264bsd_util.c",
  "source/h264bsd_util.h",
  "source/h264bsd_vlc.c",
  "source/h264bsd_vlc.h",
  "source/h264bsd_vui.c",
  "source/h264bsd_vui.h",
  "source/H264SwDecApi.c",
  "inc/H264SwDecApi.h",
  "inc/basetype.h",
  "NOTICE",
];

const baseUrl =
  "https://android.googlesource.com/platform/frameworks/av/+/refs/tags/android-8.0.0_r51/media/libstagefright/codecs/on2/h264dec/";

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function downloadFile(filename, maxRetries = 5) {
  // Check if file already exists locally
  const filePath = resolve(".", filename);
  try {
    await access(filePath);
    console.log(`Skipped (already exists): ${filename}`);
    return;
  } catch (err) {
    // File doesn't exist, continue with download
  }

  let lastError;

  for (let attempt = 1; attempt <= maxRetries; attempt++) {
    try {
      // Use the correct URL format with ?format=TEXT which returns base64
      const response = await fetch(baseUrl + filename + "?format=TEXT");
      if (!response.ok) {
        throw new Error(
          `Failed to download ${filename}: ${response.status} ${response.statusText}`,
        );
      }
      const base64Content = await response.text();

      // Decode base64 content
      const content = Buffer.from(base64Content, "base64").toString("utf-8");

      // Ensure the destination directory exists
      await mkdir(dirname(filePath), { recursive: true });

      // Write the file to the destination folder
      await writeFile(filePath, content);
      console.log(`Downloaded: ${filename}`);
      return;
    } catch (error) {
      lastError = error;
      console.warn(
        `Attempt ${attempt} failed for ${filename}: ${error.message}`,
      );
      if (attempt < maxRetries) {
        // Wait longer between retries
        await delay(2000 * attempt);
      }
    }
  }

  console.error(
    `Error downloading ${filename} after ${maxRetries} attempts:`,
    lastError.message,
  );
}

for (const filename of sourceFiles) {
  await downloadFile(filename);
}

console.log(
  `Download completed! Successfully processed ${sourceFiles.length} files.`,
);

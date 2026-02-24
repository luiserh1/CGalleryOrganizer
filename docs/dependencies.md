# Dependencies

This file documents all third-party code relied upon by the CGalleryOrganizer project, as well as the self-contained internally developed functionality that is not part of the core of the project.

**Last consulted:** 2026-02-24

---

## External Dependencies

**Rule:** All external dependencies MUST be stored in the `/vendor/` directory and must not be mixed with the core `src/` code.

### 1. cJSON
- **Purpose**: Parsing and writing the `gallery_cache.json` file.
- **Location**: `vendor/cJSON.c`, `vendor/cJSON.h`
- **License**: MIT License
- **Source**: [https://github.com/DaveGamble/cJSON](https://github.com/DaveGamble/cJSON)
- **Version**: TBD
- **Modifications**: None. Included as-is.

---

## Internal Dependencies

### 1. EXIF Parser
- **Purpose**: Extracting EXIF metadata (date taken, camera model, GPS, dimensions) from JPEG files.
- **Location**: `include/exif_parser.h`, `src/utils/exif_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written JPEG EXIF binary parser using only standard C library.
- **Format Specification**: [Library of Congress EXIF](https://www.loc.gov/preservation/digital/formats/fdd/fdd000146.shtml)
- **Binary Structure**:
  - JPEG files contain EXIF data in APP1 marker segment (0xFFE1)
  - APP1 contains: 2-byte length, "Exif\0\0", then TIFF header
  - TIFF header: 8-byte IFD header (byte order mark + magic 0x002A)
  - IFD0 contains standard tags: ImageWidth (0x0100), ImageLength (0x0101), Make (0x010F), Model (0x0110)
  - EXIF IFD pointer at tag 0x8769 contains DateTimeOriginal (0x9003), GPS IFD pointer at tag 0x8825
- **Note**: Acts as the core generic EXIF parsing engine. Used by JPEG, PNG, WebP, and HEIC parsers to extract rich metadata when available.

### 2. PNG Parser
- **Purpose**: Extracting metadata (dimensions) from PNG files.
- **Location**: `include/png_parser.h`, `src/utils/png_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written PNG binary parser using only standard C library.
- **Format Specification**: [W3C PNG Specification](https://www.w3.org/TR/png-3/)
- **Binary Structure**:
  - Signature: 8 bytes (0x89 0x50 0x4E 0x47 0x0D 0x0A 0x1A 0x0A)
  - Chunk structure: 4-byte length (big-endian) + 4-byte type + data + 4-byte CRC
  - IHDR chunk type: 0x49484452 ("IHDR")
  - IHDR data (13 bytes): Width(4) + Height(4) + BitDepth(1) + ColorType(1) + Compression(1) + Filter(1) + Interlace(1)
  - Width and Height are 4-byte big-endian integers at bytes 0-3 and 4-7 of IHDR data
- **Note**: Extracts width and height from PNG IHDR chunk. Also reads the `eXIf` chunk if present and passes it to the core EXIF parser for rich metadata.

### 3. WebP Parser
- **Purpose**: Extracting metadata (dimensions) from WebP files.
- **Location**: `include/webp_parser.h`, `src/utils/webp_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written WebP binary parser using only standard C library.
- **Format Specification**: [RFC 9649 (IETF)](https://datatracker.ietf.org/doc/rfc9649/), [Google WebP Container](https://developers.google.com/speed/webp/docs/riff_container)
- **Binary Structure**:
  - RIFF container: 4-byte "RIFF" + 4-byte file size + 4-byte "WEBP"
  - Simple format (lossy): "RIFF" + size + "WEBP" + VP8 chunk (type 0x56503820)
  - Simple format (lossless): "RIFF" + size + "WEBP" + VP8L chunk (type 0x5650384C)
  - Extended format: VP8X chunk (type 0x58385056) + flags + VP8/VP8L chunk
  - VP8X payload (10 bytes): flags (4 bytes) + width-1 (3 bytes) + height-1 (3 bytes)
  - VP8 frame: 3-byte frame tag + 7 bytes header + 3-byte scaling spec + dimensions at bytes 10-12
  - VP8L: 1-byte signature (0x2F) + 4-byte little-endian dimensions at bits 5-31
- **Note**: Extracts width and height from WebP VP8X, VP8, or VP8L chunks. Reads the `EXIF` chunk if present and passes it to the core EXIF parser.

### 4. GIF Parser
- **Purpose**: Extracting metadata (dimensions) from GIF files.
- **Location**: `include/gif_parser.h`, `src/utils/gif_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written GIF binary parser using only standard C library.
- **Format Specification**: [W3C GIF87a Specification](https://www.w3.org/Graphics/GIF/spec-gif87.txt), [Library of Congress GIF89a](https://www.loc.gov/preservation/digital/formats/fdd/fdd000133.shtml)
- **Binary Structure**:
  - Header (6 bytes): 3-byte signature "GIF" (0x47 0x49 0x46) + 3-byte version ("87a" or "89a")
  - Logical Screen Descriptor (7 bytes): CanvasWidth(2) + CanvasHeight(2) + PackedField(1) + BackgroundColor(1) + AspectRatio(1)
  - Width and Height are 16-bit little-endian integers at bytes 0-1 and 2-3 of LSD
  - Maximum dimension: 65535 x 65535 pixels
- **Note**: Extracts width and height from GIF header. Supports both GIF87a and GIF89a formats.

### 5. HEIC Parser
- **Purpose**: Extracting metadata (dimensions) from HEIC files.
- **Location**: `include/heic_parser.h`, `src/utils/heic_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written HEIC binary parser using only standard C library.
- **Format Specification**: [Wikipedia ISOBMFF](https://en.wikipedia.org/wiki/ISO_base_media_file_format), [Apple QuickTime mvhd](https://developer.apple.com/documentation/quicktime-file-format/movie_header_atom)
- **Binary Structure**:
  - File Type Box (ftyp): 4-byte size + "ftyp" (0x66747970) + 4-byte major brand + compatible brands
  - HEIC brand: "heic", "heix", "mif1", or "msf1"
  - Meta Box (meta): 4-byte size + "meta" (0x6D657461) + version/flags
  - Item Property Box (iprp): Contains Item Property Container Box (ipco)
  - Image Spatial Extents Box (ispe): 4-byte size + "ispe" (0x69737065) + version/flags + width (4 bytes, big-endian) + height (4 bytes, big-endian)
  - All boxes use 4-byte size (big-endian) + 4-byte type
- **Note**: Extracts width and height from the `ispe` box. Also scans the file for embedded `Exif\0\0` blocks to extract rich metadata.

### 6. BMP Parser
- **Purpose**: Extracting metadata (dimensions) from BMP files.
- **Location**: `include/bmp_parser.h`, `src/utils/bmp_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written BMP binary parser using only standard C library.
- **Format Specification**: [Microsoft BITMAPINFOHEADER](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader), [Library of Congress BMP v5](https://www.loc.gov/preservation/digital/formats/fdd/fdd000189)
- **Binary Structure**:
  - BITMAPFILEHEADER (14 bytes): Signature(2) "BM" + FileSize(4) + Reserved(4) + DataOffset(4)
  - BITMAPINFOHEADER (40 bytes): Size(4) + Width(4) + Height(4) + Planes(2) + BitCount(2) + Compression(4) + ImageSize(4) + XPelsPerMeter(4) + YPelsPerMeter(4) + ClrUsed(4) + ClrImportant(4)
  - Width and Height are 32-bit signed little-endian integers at bytes 18-21 and 22-25
  - Height can be negative for top-down DIBs (biHeight < 0)
- **Note**: Extracts width and height from BITMAPINFOHEADER. Supports standard BMP format.

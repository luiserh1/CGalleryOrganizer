# Test Assets

This document catalogs the test images used for integration testing, including their source, licensing, and expected metadata.

## Image Registry

| Filename | Format | Source | Expected Dimensions | Has EXIF |
|----------|--------|--------|---------|---------------------|
| sample_exif.jpg | JPEG | Flowers at Kew Gardens | 240x160 | Yes |
| sample_no_exif.jpg | JPEG | Flowers at Kew Gardens - Modified | 240x160 | No |
| sample.png | PNG | Cover of Papua New Guinean Passport | 113x161 | N/A |
| sample_exif.png | PNG | Cover of Papua New Guinean Passport - Modified | 113x161 | Yes |
| sample.webp | WebP | Drawing of a Dog Home | 400x400 | N/A |
| sample_exif.webp | WebP | Drawing of a Dog Home - Modified | 400x400 | Yes |
| sample.gif | GIF | Northern Lights timelapse | 500x331 | N/A |
| sample.heic | HEIC | Shivanasamudram falls early morning view | 256x192 | N/A |
| sample_exif.heic | HEIC | Shivanasamudram falls early morning view - Modified | 256x192 | Yes |
| sample.bmp | BMP | Checkertrd Giant rabbit | 256x151 | N/A |

## Sources

**Note on Unlisted Assets:** Any images or assets present in the `tests/assets/` directories that are not explicitly listed in this registry are either original works provided by the developer, or are directly derived from the Creative Commons licensed images listed below for testing purposes (e.g., heavily fuzzed, duplicated, truncated, or metadata-altered edge-case variants).

### Image 1: Flowers at Kew Gardens
- **Source URL**: [Flickr](https://www.flickr.com/photos/adulau/8648707725/in/photostream/)
- **License**: CC BY-SA 2.0
- **Author**: Alexandre Dulaunoy
- **Modifications**: None

### Image 2: Flowers at Kew Gardens - Modified
- **Source URL**: [Flickr](https://www.flickr.com/photos/adulau/8648707725/in/photostream/)
- **License**: CC BY-SA 2.0
- **Author**: Alexandre Dulaunoy
- **Modifications**: Metadata removed with ffmpeg

### Image 3: Cover of Papua New Guinean Passport
- **Source URL**: [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Cover_of_Papua_New_Guinean_Passport.png)
- **License**: Public domain
- **Author**: Papua New Guine government
- **Modifications**: None

### Image 4: Drawing of a Dog Home
- **Source URL**:  [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Drawing-of-a-Dog-Home-400-x-400-1.webp)
- **License**: CC BY-SA 4.0
- **Author**:  Mike Bruce
- **Modifications**: None

### Image 5: Northern Lights timelapse
- **Source URL**: [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Northern_Lights_timelapse.gif)
- **License**: CC BY-SA 4.0
- **Author**: KristianPikner
- **Modifications**: None

### Image 6: Shivanasamudram falls early morning view
- **Source URL**: [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Shivanasamudram_falls_early_morning_view.HEIC.jpg)
- **License**: CC BY-SA 3.0
- **Author**: Maskaravivek
- **Modifications**: From JPG to HEIC via default GIMP export

### Image 7: Checkered Giant rabbit
- **Source URL**: [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Checkered_Giant_rabbit.jpg)
- **License**: CC BY-SA 4.0
- **Author**: DestinationFearFan
- **Modifications**: From JPG to BMP via default GIMP export

### Image 8: Cover of Papua New Guinean Passport - Modified
- **Source URL**: [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Cover_of_Papua_New_Guinean_Passport.png)
- **License**: Public domain
- **Author**: Papua New Guine government
- **Modifications**: EXIF metadata inserted using exiftool

### Image 9: Drawing of a Dog Home - Modified
- **Source URL**:  [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Drawing-of-a-Dog-Home-400-x-400-1.webp)
- **License**: CC BY-SA 4.0
- **Author**:  Mike Bruce
- **Modifications**: EXIF metadata inserted using exiftool

### Image 10: Shivanasamudram falls early morning view - Modified
- **Source URL**: [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Shivanasamudram_falls_early_morning_view.HEIC.jpg)
- **License**: CC BY-SA 3.0
- **Author**: Maskaravivek
- **Modifications**: From JPG to HEIC via default GIMP export, then EXIF metadata inserted using exiftool

## Guidelines for Adding Test Images

1. Use only images with compatible licenses (CC0, CC BY 4.0 preferred)
2. Record source, license, and author in this document
3. Place images in appropriate `tests/assets/` subdirectory
4. Verify expected metadata before committing
5. Update the Image Registry table with actual dimensions and metadata

## Expected Metadata Format

When filling in the Image Registry, use this format for expected dimensions:
- Format: `Width x Height` (e.g., `1920 x 1080`)
- For EXIF data: Include camera model, date taken, GPS if available

## License Compatibility

### Preferred Licenses
- **CC0 1.0 Universal**: Public domain dedication
- **CC BY 4.0**: Attribution required
- **CC BY-SA 4.0**: Attribution + ShareAlike

### Avoid
- NC (Non-Commercial) licenses
- ND (No Derivatives) licenses
- Proprietary/unknown licenses

## Updating This Document

When adding new test images:
1. Add entry to Image Registry table
2. Add detailed source entry in Sources section
3. Verify file exists in correct subdirectory
4. Run tests to confirm metadata extraction works
5. Commit both image and documentation changes

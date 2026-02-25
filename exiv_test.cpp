#include <iostream>
#include <exiv2/exiv2.hpp>
int main() {
    auto image = Exiv2::ImageFactory::open("tests/assets/jpg/sample_deep_exif.jpg");
    image->readMetadata();
    auto& ed = image->exifData();
    std::cout << ed["Exif.Image.Orientation"].toLong() << "\n";
    return 0;
}

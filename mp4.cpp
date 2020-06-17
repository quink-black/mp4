#include "mp4.h"

#include <iostream>

int main(int argc, char *argv[]) {
    auto boxes = mov::Mp4Paser::parse(argv[1]);

    for (const auto &box : boxes) {
        std::cout << "type " << box->baseTypeStr()
        << ", offset " << box->offset()
        << ", size " << box->size()
        << ", " << box->detail()
        << '\n';
    }

    return 0;
}

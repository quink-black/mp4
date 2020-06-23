#include "mp4.h"

#include <stdlib.h>
#include <iostream>

void dumpBox(const mov::Box::Boxes &boxes, int depth = 0) {
    for (const auto &box : boxes) {
        std::cout << std::string(depth, '\t')
                  << "type " << box->baseTypeStr()
                  << ", offset " << box->offset()
                  << ", size " << box->size();
        bool skip_ctts = true;
        if (skip_ctts && box->baseTypeStr() == "ctts") {
            std::cout << '\n';
            continue;
        }
        std::cout << ", " << box->detail() << '\n';
        if (box->hasChild()) {
            dumpBox(box->children(), depth + 1);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "usage: " << argv[0] << " file.mp4\n";
        return EXIT_FAILURE;
    }
    auto boxes = mov::Mp4Paser::parse(argv[1]);
    dumpBox(boxes);

    return EXIT_SUCCESS;
}

#include "mp4.h"

#include <getopt.h>
#include <stdlib.h>
#include <iostream>

void dumpBox(const mov::Box::Boxes &boxes, bool verbose, int depth = 0) {
    for (const auto &box : boxes) {
        std::cout << std::string(depth * 4, ' ')
                  << "type " << box->boxTypeStr()
                  << ", offset " << box->offset()
                  << ", size " << box->size();
        auto detail = box->detail();
        if (verbose) {
            std::cout << ", " << box->detail() << '\n';
        } else {
            auto second_newline = detail.find('\n', detail.find('\n') + 1);
            if (second_newline != std::string::npos) {
                std::cout << ", " << detail.substr(0, second_newline)
                          << " ...\n";
            } else {
                std::cout << detail << '\n';
            }
        }
        if (box->hasChild()) {
            dumpBox(box->children(), verbose, depth + 1);
        }
    }
}

static void usage(const char *arg0) {
    std::cout << "usage: " << arg0 << " -v file.mp4\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    int ch = 0;
    bool verbose = false;

    while ((ch = getopt(argc, argv, "v")) != -1) {
        switch (ch) {
            case 'v':
                verbose = true;
                break;
            case '?':
            default:
                usage(argv[0]);
                return EXIT_SUCCESS;
        }
    }

    argc -= optind;
    argv += optind;
    auto boxes = mov::Mp4Paser::parse(*argv);
    dumpBox(boxes, verbose);

    return EXIT_SUCCESS;
}

/* Project-authored test-only driver for a separate external LGPL OpenV2G
 * executable. Never linked into openv2g_core or the generated candidate.
 */
#include <stdio.h>
#include <string.h>
#include "appHandEXIDatatypesDecoder.h"
#include "appHandEXIDatatypesEncoder.h"
#include "dinEXIDatatypesDecoder.h"
#include "dinEXIDatatypesEncoder.h"
#include "iso1EXIDatatypesDecoder.h"
#include "iso1EXIDatatypesEncoder.h"
#include "BitOutputStream.h"
int main(int argc, char **argv)
{
    uint8_t input[4097] = {0}, output[4097] = {0};
    size_t length, read_position = 0, write_position = 0;
    bitstream_t reader, writer;
    int result = -1;
    if (argc != 2) { return 2; }
    length = fread(input, 1, sizeof(input), stdin);
    if (length == 0 || length >= sizeof(input)) { return 2; }
    (void)memset(&reader, 0, sizeof(reader)); (void)memset(&writer, 0, sizeof(writer));
    reader.data = input; reader.size = length; reader.pos = &read_position;
    writer.data = output; writer.size = sizeof(output); writer.pos = &write_position; writer.capacity = 8;
    if (strcmp(argv[1], "sap") == 0) {
        struct appHandEXIDocument document;
        (void)memset(&document, 0, sizeof(document));
        result = decode_appHandExiDocument(&reader, &document);
        if (result == 0) { result = encode_appHandExiDocument(&writer, &document); }
    } else if (strcmp(argv[1], "din") == 0) {
        struct dinEXIDocument document;
        (void)memset(&document, 0, sizeof(document));
        result = decode_dinExiDocument(&reader, &document);
        if (result == 0) { result = encode_dinExiDocument(&writer, &document); }
    } else if (strcmp(argv[1], "iso2-fragment") == 0) {
        struct iso1EXIFragment fragment;
        (void)memset(&fragment, 0, sizeof(fragment));
        result = decode_iso1ExiFragment(&reader, &fragment);
        if (result == 0) { result = encode_iso1ExiFragment(&writer, &fragment); }
    } else if (strcmp(argv[1], "iso2") == 0) {
        /* External OpenV2G calls the 2013 model iso1; iso2 is a different
         * model disabled in that pinned checkout. Do not conflate names. */
        struct iso1EXIDocument document;
        (void)memset(&document, 0, sizeof(document));
        result = decode_iso1ExiDocument(&reader, &document);
        if (result == 0) { result = encode_iso1ExiDocument(&writer, &document); }
    }
    if (result != 0) { (void)fprintf(stderr, "oracle result=%d\n", result); return 1; }
    if (writer.capacity != 8 && flush(&writer) != 0) { return 1; }
    return fwrite(output, 1, write_position, stdout) == write_position ? 0 : 1;
}

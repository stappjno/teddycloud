#ifdef WIN32
#else
#include <unistd.h>
#endif

#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#include "handler_sse.h"
#include "handler_rtnl.h"
#include "settings.h"
#include "stats.h"
#include "cloud_request.h"

#include "proto/toniebox.pb.rtnl.pb-c.h"

FsFile *fsOpenFile2(const char_t *path, char *mode);
FsFile *fsOpenFile2(const char_t *path, char *mode)
{
    // Workaround due to missing append in cyclone framwwork.

    // File pointer
    FILE *fp = NULL;

    // Make sure the pathname is valid
    if (path == NULL)
        return NULL;

    // Open the specified file
    fp = fopen(path, mode);

    // Return a handle to the file
    return fp;
}

static void escapeString(const char_t *input, size_t size, char_t *output);
static void escapeString(const char_t *input, size_t size, char_t *output)
{
    // Replacement sequences for special characters
    const char_t *replacements[] = {
        "\"", "\"\"", // Double quote (")
        "\n", "\\n",  // Newline
        "\r", "\\r"   // Carriage return
    };
    const size_t num_replacements = sizeof(replacements) / sizeof(replacements[0]);

    size_t input_length = size;
    size_t escaped_length = 0;

    // First pass to count the number of additional characters required for escaping
    for (size_t i = 0; i < input_length; i++)
    {
        for (size_t j = 0; j < num_replacements; j++)
        {
            if (input[i] == replacements[j][0])
            {
                escaped_length += osStrlen(replacements[j]) - 1;
                break;
            }
        }
    }

    size_t j = 0;
    // Second pass to actually escape the characters
    for (size_t i = 0; i < input_length; i++)
    {
        bool_t replaced = false;
        for (size_t k = 0; k < num_replacements; k++)
        {
            if (input[i] == replacements[k][0])
            {
                size_t len = osStrlen(replacements[k]);
                osStrcpy(&output[j], replacements[k]);
                j += len;
                replaced = true;
                break;
            }
        }

        if (!replaced)
        {
            output[j++] = input[i];
        }
    }

    // Null-terminate the escaped string
    output[j] = '\0';
}

error_t handleRtnl(HttpConnection *connection, const char_t *uri, const char_t *queryString)
{
    char_t *data = connection->buffer;
    size_t size = connection->response.contentLength;

    if (get_settings()->rtnl.logRaw)
    {
        FsFile *file = fsOpenFile2(get_settings()->rtnl.logRawFile, "ab");
        fsWriteFile(file, &data[connection->response.byteCount], size);
        fsCloseFile(file);
    }

    size_t pos = 0;
    while (size > 4 && pos < (size - 4))
    {
        data = &connection->buffer[pos];
        uint32_t protoLength = (uint32_t)((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
        char_t *protoData = &data[4];
        if (protoLength > (size - 4 - pos))
        {
            break;
        }
        if (protoLength == 0 || (protoLength + 4 > HTTP_SERVER_BUFFER_SIZE)) // find apropiate size
        {
            TRACE_WARNING("Invalid protoLen=%" PRIu32 ", pos=%" PRIuSIZE "\r\n", protoLength, pos);
            pos++;
            continue;
        }
        TonieRtnlRPC *rpc = tonie_rtnl_rpc__unpack(NULL, protoLength, (const uint8_t *)protoData);

        pos += protoLength + 4;
        if (rpc && (rpc->log2 || rpc->log3))
        {
            rtnlEvent(rpc);
            rtnlEventLog(rpc);
            rtnlEventDump(rpc);
        }
        tonie_rtnl_rpc__free_unpacked(rpc, NULL);
    }

    osMemcpy(connection->buffer, data, size - pos);
    connection->response.byteCount = size - pos;

    return NO_ERROR;
}

void rtnlEvent(TonieRtnlRPC *rpc)
{
    char_t buffer[4096];
    if (rpc->log2)
    {
        sse_startEventRaw("rtnl-raw-log2");
        osSprintf(buffer, "\"uptime\": %" PRIu64 ", "
                          "\"sequence\": %" PRIu32 ", "
                          "\"field3\": %" PRIu32 ", "
                          "\"function_group\": %" PRIu32 ", "
                          "\"function\": %" PRIu32 ", "
                          "\"field6\": \"",
                  rpc->log2->uptime,
                  rpc->log2->sequence,
                  rpc->log2->field3,
                  rpc->log2->function_group,
                  rpc->log2->function);
        sse_rawData(buffer);

        if (rpc->log2->field6.len > 0)
        {
            for (size_t i = 0; i < rpc->log2->field6.len; i++)
            {
                osSprintf(&buffer[i * 2], "%02X", rpc->log2->field6.data[i]);
            }
            sse_rawData(buffer);
        }

        osSprintf(buffer, "\","
                          "\"field8\": %" PRIu32 ", "
                          "\"field9\": \"",
                  rpc->log2->field8);
        sse_rawData(buffer);

        if (rpc->log2->field9.len > 0)
        {
            for (size_t i = 0; i < rpc->log2->field9.len; i++)
            {
                osSprintf(&buffer[i * 2], "%02X", rpc->log2->field9.data[i]);
            }
            sse_rawData(buffer);
        }
        sse_rawData("\"");

        sse_endEventRaw();
    }

    if (rpc->log3)
    {
        sse_startEventRaw("rtnl-raw-log3");
        osSprintf(buffer, "\"datetime\": %" PRIu32 ", "
                          "\"field2\": %" PRIu32,
                  rpc->log3->datetime,
                  rpc->log3->field2);
        sse_rawData(buffer);
        sse_endEventRaw();
    }

    if (rpc->log3)
    {
        if (rpc->log3->field2 == 1)
        {
            sse_sendEvent("pressed", "ear-big", true);
        }
        else if (rpc->log3->field2 == 2)
        {
            sse_sendEvent("pressed", "ear-small", true);
        }
    }
}

void rtnlEventLog(TonieRtnlRPC *rpc)
{
    TRACE_DEBUG("RTNL: \r\n");
    if (rpc->log2)
    {
        TRACE_DEBUG(" LOG2:\r\n");
        TRACE_DEBUG("  uptime=%" PRIu64 "\r\n", rpc->log2->uptime);
        TRACE_DEBUG("  sequence=%" PRIu32 "\r\n", rpc->log2->sequence);
        TRACE_DEBUG("  3=%" PRIu32 "\r\n", rpc->log2->field3);
        TRACE_DEBUG("  group=%" PRIu32 "\r\n", rpc->log2->function_group);
        TRACE_DEBUG("  function=%" PRIu32 "\r\n", rpc->log2->function);
        TRACE_DEBUG("  6=len(data)=%" PRIuSIZE ", data=", rpc->log2->field6.len);
        for (size_t i = 0; i < rpc->log2->field6.len; i++)
        {
            TRACE_DEBUG_RESUME("%02X", rpc->log2->field6.data[i]);
        }
        TRACE_DEBUG_RESUME(", txt=%s\r\n", rpc->log2->field6.data);
        if (rpc->log2->has_field8)
            TRACE_DEBUG("  8=%" PRIu32 "\r\n", rpc->log2->field8);
        if (rpc->log2->has_field9)
        {
            TRACE_DEBUG("  9=len(data)=%" PRIuSIZE ", data=", rpc->log2->field9.len);
            for (size_t i = 0; i < rpc->log2->field9.len; i++)
            {
                TRACE_DEBUG_RESUME("%02X", rpc->log2->field9.data[i]);
            }
            TRACE_DEBUG_RESUME(", txt=%s\r\n", rpc->log2->field9.data);
        }
    }
    if (rpc->log3)
    {
        TRACE_DEBUG(" LOG3:\r\n");
        TRACE_DEBUG("  datetime=%" PRIu32 "\r\n", rpc->log3->datetime);
        TRACE_DEBUG("  2=%" PRIu32 "\r\n", rpc->log3->field2);
    }
}
void rtnlEventDump(TonieRtnlRPC *rpc)
{
    if (get_settings()->rtnl.logHuman)
    {
        bool_t addHeader = !fsFileExists(get_settings()->rtnl.logHumanFile);
        FsFile *file = fsOpenFile2(get_settings()->rtnl.logHumanFile, "ab");
        if (addHeader)
        {
            char_t *header = "timestamp;log2;uptime;sequence;3;group;function;6(len);6(bytes);6(string);8;9(len);9(bytes);9(string);log3;datetime;2\r\n";
            fsWriteFile(file, header, osStrlen(header));
        }
        char_t buffer[4096];
        osSprintf(buffer, "%" PRIuTIME ";", time(NULL));
        fsWriteFile(file, buffer, osStrlen(buffer));

        if (rpc->log2)
        {
            osSprintf(buffer, "x;%" PRIu64 ";%" PRIu32 ";%" PRIu32 ";%" PRIu32 ";%" PRIu32 ";%" PRIuSIZE ";",
                      rpc->log2->uptime,
                      rpc->log2->sequence,
                      rpc->log2->field3,
                      rpc->log2->function_group,
                      rpc->log2->function,
                      rpc->log2->field6.len);
            fsWriteFile(file, buffer, osStrlen(buffer));

            if (rpc->log2->field6.len > 0)
            {
                for (size_t i = 0; i < rpc->log2->field6.len; i++)
                {
                    osSprintf(&buffer[i * 2], "%02X", rpc->log2->field6.data[i]);
                }
                fsWriteFile(file, buffer, osStrlen(buffer));
            }

            osSprintf(buffer, ";\"");
            escapeString((char_t *)rpc->log2->field6.data, rpc->log2->field6.len, &buffer[2]);
            fsWriteFile(file, buffer, osStrlen(buffer));

            osSprintf(buffer, "\";%" PRIu32 ";%" PRIuSIZE ";",
                      rpc->log2->field8, // TODO hasfield
                      rpc->log2->field9.len);
            fsWriteFile(file, buffer, osStrlen(buffer));

            if (rpc->log2->has_field9)
            {
                if (rpc->log2->field9.len > 0)
                {
                    for (size_t i = 0; i < rpc->log2->field9.len; i++)
                    {
                        osSprintf(&buffer[i * 2], "%02X", rpc->log2->field9.data[i]);
                    }
                    fsWriteFile(file, buffer, osStrlen(buffer));
                }
                osSprintf(buffer, ";\"");
                escapeString((char_t *)rpc->log2->field9.data, rpc->log2->field9.len, &buffer[2]);
                fsWriteFile(file, buffer, osStrlen(buffer));
                char_t *output = "\";";
                fsWriteFile(file, output, osStrlen(output));
            }
            else
            {
                char_t *output = ";;";
                fsWriteFile(file, output, osStrlen(output));
            }
        }
        else
        {
            char_t *output = ";;;;;;;;;;;;;";
            fsWriteFile(file, output, osStrlen(output));
        }

        if (rpc->log3)
        {
            osSprintf(buffer, "x;%" PRIu32 ";%" PRIu32 "\r\n",
                      rpc->log3->datetime,
                      rpc->log3->field2);
            fsWriteFile(file, buffer, osStrlen(buffer));
        }
        else
        {
            char_t *output = ";;\r\n";
            fsWriteFile(file, output, osStrlen(output));
        }
        fsCloseFile(file);
    }
}
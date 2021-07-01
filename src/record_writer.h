/*
 * zsim's ability to record large-scale binary data such as histograms is
 * currently a bit lacking. So, we implement the RecordWriter class. In contrast
 * to the Stats classes, RecordWriter records are designed for non-fixed-size
 * information (even zsim's "TBD" histogram only supports a fixed number of
 * buckets).
 *
 * Users register a lambda function that gets called at simulation end which
 * dumps the record to a file.
 */

#ifndef RECORD_WRITER_H_
#define RECORD_WRITER_H_

#include <fstream>
#include <functional>

#include "g_std/g_string.h"


class RecordWriter : public GlobAlloc {
    public:
        RecordWriter(const g_string& name, std::function<void (std::ofstream&)>
                write_record);
        RecordWriter(const RecordWriter& rw) = delete;
        RecordWriter& operator=(const RecordWriter& rw) = delete;
        RecordWriter(RecordWriter&& rw) = delete;
        RecordWriter& operator=(RecordWriter&& rw) = delete;
        ~RecordWriter();

        void dump(const g_string& output_dir);

    private:
        g_string name;
        std::function<void (std::ofstream&)> write_record;
};


#endif  // RECORD_WRITER_H_

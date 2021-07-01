#include "record_writer.h"


RecordWriter::RecordWriter(const g_string& name,
        std::function<void (std::ofstream&)> write_record) : name(name),
        write_record(write_record)
{
}

RecordWriter::~RecordWriter()
{
}

void
RecordWriter::dump(const g_string& output_dir)
{
    g_string _filepath = output_dir + "/" + name + ".bin";
    std::string filepath = _filepath.c_str();

    std::ofstream ofs(filepath, std::ios::out | std::ios::binary);

    write_record(ofs);
}

#include "pipeline/Pipeline.h"

Pipeline::Pipeline(IRecordReader& reader, IGpsFilter& filter, IOutput& output)
    : m_reader{reader}
    , m_filter{filter}
    , m_output{output}
{}

void Pipeline::run()
{
    while (m_reader.hasNext())
    {
        auto [point, error] = m_reader.readNext();

        if (!point.has_value())
        {
            if (!error.empty())
                m_output.writeError(error);
            continue;
        }

        GpsPoint& pt = *point;
        auto result  = m_filter.process(pt);

        if (result.status == FilterStatus::Reject)
            m_output.writeRejected(pt, result.reason);
        else
            m_output.writePoint(pt);
    }
}

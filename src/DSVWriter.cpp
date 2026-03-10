#include "DSVWriter.h"

struct CDSVWriter::SImplementation {
    std::shared_ptr<CDataSink> DSink;
    char DDelimiter;
    bool DQuoteAll;
};

CDSVWriter::CDSVWriter(std::shared_ptr<CDataSink> sink, char delimiter, bool quoteall)
    : DImplementation(std::make_unique<SImplementation>()) {
    DImplementation->DSink = sink;
    // quote char can't be a delimiter, fall back to comma
    DImplementation->DDelimiter = (delimiter == '"') ? ',' : delimiter;
    DImplementation->DQuoteAll = quoteall;
}

CDSVWriter::~CDSVWriter() = default;

bool CDSVWriter::WriteRow(const std::vector<std::string> &row) {
    for(size_t i = 0; i < row.size(); i++){
        const std::string &field = row[i];
        // need to quote if field has special chars or quoteall is set
        bool needsQuote = DImplementation->DQuoteAll ||
            field.find(DImplementation->DDelimiter) != std::string::npos ||
            field.find('"') != std::string::npos ||
            field.find('\n') != std::string::npos;

        if(needsQuote){
            DImplementation->DSink->Put('"');
            for(char ch : field){
                if(ch == '"') DImplementation->DSink->Put('"'); // escape quotes by doubling
                DImplementation->DSink->Put(ch);
            }
            DImplementation->DSink->Put('"');
        }
        else{
            for(char ch : field){
                DImplementation->DSink->Put(ch);
            }
        }

        // delimiter goes between fields not after the last one
        if(i < row.size() - 1){
            DImplementation->DSink->Put(DImplementation->DDelimiter);
        }
    }
    DImplementation->DSink->Put('\n');
    return true;
}

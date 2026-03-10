#include "DSVReader.h"

struct CDSVReader::SImplementation {
    std::shared_ptr<CDataSource> DSource;
    char DDelimiter;
    bool DEnd;
};

CDSVReader::CDSVReader(std::shared_ptr<CDataSource> src, char delimiter)
    : DImplementation(std::make_unique<SImplementation>()) {
    DImplementation->DSource = src;
    // quote char can't be a delimiter, fall back to comma
    DImplementation->DDelimiter = (delimiter == '"') ? ',' : delimiter;
    DImplementation->DEnd = false;
}

CDSVReader::~CDSVReader() = default;

bool CDSVReader::End() const {
    return DImplementation->DEnd;
}

bool CDSVReader::ReadRow(std::vector<std::string> &row) {
    row.clear();
    if(DImplementation->DSource->End()){
        DImplementation->DEnd = true;
        return false;
    }

    std::string field;
    // tracks whether we've seen anything besides a bare newline
    bool seenContent = false;

    while(true){
        if(DImplementation->DSource->End()){
            row.push_back(field);
            DImplementation->DEnd = true;
            return true;
        }

        char ch;
        DImplementation->DSource->Peek(ch);

        if(ch == '\n'){
            DImplementation->DSource->Get(ch);
            // check eof right after newline so End() reflects state immediately
            if(DImplementation->DSource->End()) DImplementation->DEnd = true;
            // bare newline = empty row, but if we saw content push the field
            if(!seenContent && field.empty()){
                return true;
            }
            row.push_back(field);
            return true;
        }
        // quoted field — read until closing quote
        else if(ch == '"'){
            DImplementation->DSource->Get(ch);
            seenContent = true;
            while(true){
                if(DImplementation->DSource->End()) break;
                DImplementation->DSource->Get(ch);
                if(ch == '"'){
                    char next;
                    // "" inside quotes is an escaped literal quote
                    if(!DImplementation->DSource->End() && DImplementation->DSource->Peek(next) && next == '"'){
                        DImplementation->DSource->Get(next);
                        field += '"';
                    }
                    else{
                        break;
                    }
                }
                else{
                    field += ch;
                }
            }
        }
        else if(ch == DImplementation->DDelimiter){
            DImplementation->DSource->Get(ch);
            row.push_back(field);
            field.clear();
            seenContent = true;
        }
        else{
            DImplementation->DSource->Get(ch);
            field += ch;
            seenContent = true;
        }
    }
}

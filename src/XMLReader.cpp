#include "XMLReader.h"

#include <expat.h>

#include <deque>
#include <string>
#include <vector>

struct CXMLReader::SImplementation {
    std::shared_ptr<CDataSource> DSource;
    XML_Parser DParser;
    std::deque<SXMLEntity> DQueue;
    bool DParsedFinal;

    // Buffer character data between element callbacks
    std::string DCharBuffer;

    SImplementation(std::shared_ptr<CDataSource> src)
        : DSource(src), DParser(nullptr), DParsedFinal(false) {

        DParser = XML_ParserCreate(nullptr);
        XML_SetUserData(DParser, this);

        XML_SetElementHandler(DParser, StartElementHandler, EndElementHandler);
        XML_SetCharacterDataHandler(DParser, CharacterDataHandler);
    }

    ~SImplementation() {
        if (DParser) {
            XML_ParserFree(DParser);
            DParser = nullptr;
        }
    }

    static void StartElementHandler(void *userdata, const XML_Char *name, const XML_Char **atts) {
        auto *impl = static_cast<SImplementation *>(userdata);

        // Flush any pending char data before starting a new element
        impl->FlushCharDataToQueue();

        SXMLEntity ent;
        ent.DType = SXMLEntity::EType::StartElement;
        ent.DNameData = name;

        for (int i = 0; atts && atts[i]; i += 2) {
            std::string key = atts[i];
            std::string val = atts[i + 1] ? atts[i + 1] : "";
            ent.DAttributes.push_back(std::make_pair(key, val));
        }

        impl->DQueue.push_back(ent);
    }

    static void EndElementHandler(void *userdata, const XML_Char *name) {
        auto *impl = static_cast<SImplementation *>(userdata);

        // Flush any pending char data before ending an element
        impl->FlushCharDataToQueue();

        // Checking if the last entity was a start element with the same name
        if (!impl->DQueue.empty()) {
            const SXMLEntity &last = impl->DQueue.back();
            if (last.DType == SXMLEntity::EType::StartElement && last.DNameData == name) {
                impl->DQueue.back().DType = SXMLEntity::EType::CompleteElement; 
                return;
            }
        }

        SXMLEntity ent;
        ent.DType = SXMLEntity::EType::EndElement;
        ent.DNameData = name;
        impl->DQueue.push_back(ent);
    }

    static void CharacterDataHandler(void *userdata, const XML_Char *s, int len) {
        auto *impl = static_cast<SImplementation *>(userdata);
        if (s && len > 0) {
            impl->DCharBuffer.append(s, s + len);
        }
    }

    void FlushCharDataToQueue() {
        if (!DCharBuffer.empty()) {
            SXMLEntity ent;
            ent.DType = SXMLEntity::EType::CharData;
            ent.DNameData = DCharBuffer;
            DQueue.push_back(ent);
            DCharBuffer.clear();
        }
    }

    bool ParseMore() {
        if (DParsedFinal) {
            return false;
        }

        std::vector<char> buf;
        bool got = DSource->Read(buf, 4096);

        if (got) {
            int ok = XML_Parse(DParser, buf.data(), static_cast<int>(buf.size()), 0);
            return ok != 0;
        } else {
            // If bytes read: finalize parsing
            int ok = XML_Parse(DParser, "", 0, 1);
            DParsedFinal = true;

            // Flush any remaining char data 
            FlushCharDataToQueue();

            return ok != 0;
        }
    }
};

CXMLReader::CXMLReader(std::shared_ptr<CDataSource> src)
    : DImplementation(std::make_unique<SImplementation>(src)) {}

CXMLReader::~CXMLReader() = default;

bool CXMLReader::End() const {
    // Finished if the source is at EOF and nothing queued
    return DImplementation->DSource->End() && DImplementation->DQueue.empty()
           && DImplementation->DCharBuffer.empty();
}


bool CXMLReader::ReadEntity(SXMLEntity &entity, bool skipcdata) {
    while (true) {
        // REturn if somehting queued
        while (!DImplementation->DQueue.empty()) {
            SXMLEntity next = DImplementation->DQueue.front();
            DImplementation->DQueue.pop_front();

            if (skipcdata && next.DType == SXMLEntity::EType::CharData) {
                continue;
            }

            entity = next;
            return true;
        }

        // Parse is finished if nothing is queued
        if (DImplementation->DParsedFinal) {
            return false;
        }

        // Otherwise parse more input into the queue
        if (!DImplementation->ParseMore()) {
            // Parse error or nothing more to parse
            if (DImplementation->DParsedFinal && DImplementation->DQueue.empty()) {
                return false;
            }
            // Return false if a parse error occurred
            return false;
        }
    }
}

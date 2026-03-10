.DEFAULT_GOAL := all
CXX = g++

ifneq ($(wildcard /opt/homebrew/include/gtest/gtest.h),)
EXTRA_INC = -I /opt/homebrew/include -I /opt/homebrew/opt/expat/include
EXTRA_LIB = -L /opt/homebrew/lib -L /opt/homebrew/opt/expat/lib
endif

CXXFLAGS = -std=c++20 -fprofile-arcs -ftest-coverage -I include $(EXTRA_INC)
LDFLAGS = -lgtest -lgtest_main -lgmock -lgmock_main -lpthread -fprofile-arcs -ftest-coverage $(EXTRA_LIB)

.PHONY: all test coverage clean dirs

test: dirs \
	testbin/teststrutils testbin/teststrdatasource testbin/teststrdatasink \
	testbin/testfiledatass testbin/testdsv testbin/testxml testbin/testkml \
	testbin/testcsvbs testbin/testosm testbin/testdpr testbin/testcsvbsi \
	testbin/testtp testbin/testtpcl
	./testbin/teststrutils
	./testbin/teststrdatasource
	./testbin/teststrdatasink
	./testbin/testfiledatass
	./testbin/testdsv
	./testbin/testxml
	./testbin/testkml
	./testbin/testcsvbs
	./testbin/testosm
	./testbin/testdpr
	./testbin/testcsvbsi
	./testbin/testtp
	./testbin/testtpcl

all: test bin/transplanner bin/speedtest

coverage: test
	@gcov obj/*.o >/dev/null 2>&1 || true
	@lcov --capture --directory obj --output-file coverage.info --ignore-errors inconsistent,unsupported >/dev/null 2>&1 || true
	@lcov --remove coverage.info '/Library/*' '/opt/homebrew/*' '*/googletest/*' '*/testsrc/*' \
		--output-file coverage.filtered.info --ignore-errors unused,inconsistent,unsupported >/dev/null 2>&1 || true
	@test -f coverage.filtered.info && genhtml coverage.filtered.info --output-directory htmlcov \
		--ignore-errors inconsistent,corrupt,unsupported,category >/dev/null 2>&1 || true

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

testobj/%.o: testsrc/%.cpp | testobj
	$(CXX) $(CXXFLAGS) -c $< -o $@

testbin/teststrutils: obj/StringUtils.o testobj/StringUtilsTest.o
	$(CXX) $^ $(LDFLAGS) -o $@

testbin/teststrdatasource: obj/StringDataSource.o testobj/StringDataSourceTest.o
	$(CXX) $^ $(LDFLAGS) -o $@

testbin/teststrdatasink: obj/StringDataSink.o testobj/StringDataSinkTest.o
	$(CXX) $^ $(LDFLAGS) -o $@

testbin/testfiledatass: obj/FileDataSource.o obj/FileDataSink.o obj/FileDataFactory.o testobj/FileDataSSTest.o
	$(CXX) $^ $(LDFLAGS) -o $@

testbin/testdsv: obj/DSVReader.o obj/DSVWriter.o obj/StringDataSource.o obj/StringDataSink.o obj/StringUtils.o testobj/DSVTest.o
	$(CXX) $^ $(LDFLAGS) -o $@

testbin/testxml: obj/XMLReader.o obj/XMLWriter.o obj/StringDataSource.o obj/StringDataSink.o testobj/XMLTest.o
	$(CXX) $^ $(LDFLAGS) -lexpat -o $@

testbin/testkml: obj/KMLWriter.o obj/StringDataSink.o obj/StringDataSource.o obj/XMLWriter.o obj/StringUtils.o testobj/KMLTest.o
	$(CXX) $^ $(LDFLAGS) -lexpat -o $@

testbin/testcsvbs: obj/CSVBusSystem.o obj/DSVReader.o obj/StringDataSource.o obj/StringDataSink.o testobj/CSVBusSystemTest.o
	$(CXX) $^ $(LDFLAGS) -o $@

testbin/testosm: obj/OpenStreetMap.o obj/XMLReader.o obj/StringDataSource.o obj/StringDataSink.o testobj/OSMTest.o
	$(CXX) $^ $(LDFLAGS) -lexpat -o $@

testbin/testdpr: obj/DijkstraPathRouter.o testobj/DijkstraPathRouterTest.o
	$(CXX) $^ $(LDFLAGS) -o $@

testbin/testcsvbsi: obj/BusSystemIndexer.o obj/CSVBusSystem.o obj/DSVReader.o obj/StringDataSource.o obj/StringDataSink.o testobj/CSVBusSystemIndexerTest.o
	$(CXX) $^ $(LDFLAGS) -o $@

testbin/testtp: obj/DijkstraTransportationPlanner.o obj/DijkstraPathRouter.o obj/BusSystemIndexer.o \
	obj/OpenStreetMap.o obj/XMLReader.o obj/CSVBusSystem.o obj/DSVReader.o \
	obj/StringDataSource.o obj/StringDataSink.o obj/GeographicUtils.o \
	testobj/CSVOSMTransportationPlannerTest.o
	$(CXX) $^ $(LDFLAGS) -lexpat -o $@

testbin/testtpcl: obj/TransportationPlannerCommandLine.o obj/StringDataSource.o obj/StringDataSink.o \
	obj/DSVWriter.o testobj/TPCommandLineTest.o
	$(CXX) $^ $(LDFLAGS) -o $@

bin/transplanner: obj/DijkstraTransportationPlanner.o obj/DijkstraPathRouter.o obj/BusSystemIndexer.o \
	obj/OpenStreetMap.o obj/XMLReader.o obj/CSVBusSystem.o obj/DSVReader.o obj/DSVWriter.o \
	obj/StringDataSource.o obj/StringDataSink.o obj/GeographicUtils.o \
	obj/FileDataSource.o obj/FileDataSink.o obj/FileDataFactory.o \
	obj/StandardDataSource.o obj/StandardDataSink.o obj/StandardErrorDataSink.o \
	obj/TransportationPlannerCommandLine.o obj/KMLWriter.o obj/XMLWriter.o
	$(CXX) $^ $(LDFLAGS) -lexpat -o $@

bin/speedtest: obj/DijkstraTransportationPlanner.o obj/DijkstraPathRouter.o obj/BusSystemIndexer.o \
	obj/OpenStreetMap.o obj/XMLReader.o obj/CSVBusSystem.o obj/DSVReader.o \
	obj/StringDataSource.o obj/StringDataSink.o obj/GeographicUtils.o \
	obj/FileDataSource.o obj/FileDataSink.o obj/FileDataFactory.o \
	obj/StandardDataSource.o obj/StandardDataSink.o obj/StandardErrorDataSink.o \
	obj/speedtest.o
	$(CXX) $^ $(LDFLAGS) -lexpat -o $@

dirs:
	mkdir -p bin htmlcov lib obj testbin testobj testtmp

clean:
	rm -rf bin htmlcov lib obj testbin testobj testtmp *.gcda *.gcno *.info *.gcov

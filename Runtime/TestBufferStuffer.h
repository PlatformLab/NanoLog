#ifndef BUFFER_STUFFER
#define BUFFER_STUFFER

#include "Packer.h"
#include "FastLogger.h"

// Record code (for debugging)

namespace {
void __syang0__fl__1() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 1, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__2() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 2, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__3(int arg0, int arg1, double arg2, const char* arg3) {
	int str3Len = strlen(arg3) + 1;
	int maxSizeOfArgs = sizeof(arg0) + sizeof(arg1) + sizeof(arg2) + str3Len;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 3, maxSizeOfArgs, 2);
	char *buffer = re->argData;

	BufferUtils::recordPrimitive(buffer, arg0);
	BufferUtils::recordPrimitive(buffer, arg1);
	BufferUtils::recordPrimitive(buffer, arg2);

	memcpy(buffer, arg3, str3Len); buffer += str3Len;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__4() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 4, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__5() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 5, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__6(const char* arg0) {
	int str0Len = strlen(arg0) + 1;
	int maxSizeOfArgs = 0 + str0Len;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 6, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	memcpy(buffer, arg0, str0Len); buffer += str0Len;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__7() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 7, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__8(int arg0) {
	int maxSizeOfArgs = sizeof(arg0) + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 8, maxSizeOfArgs, 1);
	char *buffer = re->argData;

	BufferUtils::recordPrimitive(buffer, arg0);

	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__9(int arg0) {
	int maxSizeOfArgs = sizeof(arg0) + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 9, maxSizeOfArgs, 1);
	char *buffer = re->argData;

	BufferUtils::recordPrimitive(buffer, arg0);

	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__10() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 10, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__11() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 11, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__12() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 12, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__13() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 13, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__14(const char* arg0) {
	int str0Len = strlen(arg0) + 1;
	int maxSizeOfArgs = 0 + str0Len;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 14, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	memcpy(buffer, arg0, str0Len); buffer += str0Len;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__15() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 15, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__16(const char* arg0) {
	int str0Len = strlen(arg0) + 1;
	int maxSizeOfArgs = 0 + str0Len;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 16, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	memcpy(buffer, arg0, str0Len); buffer += str0Len;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__17(const char* arg0) {
	int str0Len = strlen(arg0) + 1;
	int maxSizeOfArgs = 0 + str0Len;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 17, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	memcpy(buffer, arg0, str0Len); buffer += str0Len;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__18(int arg0) {
	int maxSizeOfArgs = sizeof(arg0) + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 18, maxSizeOfArgs, 1);
	char *buffer = re->argData;

	BufferUtils::recordPrimitive(buffer, arg0);

	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__19() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 19, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__20() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 20, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__21() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 21, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__22() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 22, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__23() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 23, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

void __syang0__fl__24() {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::alloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 24, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

} // end empty namespace

// Compression Code
inline void
compressArgs1(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs2(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs3(BufferUtils::RecordEntry *re, char** out) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(*out);
	*out += 2;
	char* args = re->argData;
	int arg0 = *reinterpret_cast<int*>(args); args += sizeof(int);
	int arg1 = *reinterpret_cast<int*>(args); args += sizeof(int);
	double arg2 = *reinterpret_cast<double*>(args); args += sizeof(double);

	nib[0].first = BufferUtils::pack(out, arg0);
	nib[0].second = BufferUtils::pack(out, arg1);
	nib[1].first = BufferUtils::pack(out, arg2);

	int stringBytes = re->entrySize - (sizeof(arg0) + sizeof(arg1) + sizeof(arg2)) - 2;
	memcpy(*out, args, stringBytes);
	args += stringBytes;
	out += stringBytes;
}

inline void
compressArgs4(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs5(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs6(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;


	int stringBytes = re->entrySize - (0) - 0;
	memcpy(*out, args, stringBytes);
	args += stringBytes;
	out += stringBytes;
}

inline void
compressArgs7(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs8(BufferUtils::RecordEntry *re, char** out) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(*out);
	*out += 1;
	char* args = re->argData;
	int arg0 = *reinterpret_cast<int*>(args); args += sizeof(int);

	nib[0].first = BufferUtils::pack(out, arg0);
}

inline void
compressArgs9(BufferUtils::RecordEntry *re, char** out) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(*out);
	*out += 1;
	char* args = re->argData;
	int arg0 = *reinterpret_cast<int*>(args); args += sizeof(int);

	nib[0].first = BufferUtils::pack(out, arg0);
}

inline void
compressArgs10(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs11(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs12(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs13(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs14(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;


	int stringBytes = re->entrySize - (0) - 0;
	memcpy(*out, args, stringBytes);
	args += stringBytes;
	out += stringBytes;
}

inline void
compressArgs15(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs16(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;


	int stringBytes = re->entrySize - (0) - 0;
	memcpy(*out, args, stringBytes);
	args += stringBytes;
	out += stringBytes;
}

inline void
compressArgs17(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;


	int stringBytes = re->entrySize - (0) - 0;
	memcpy(*out, args, stringBytes);
	args += stringBytes;
	out += stringBytes;
}

inline void
compressArgs18(BufferUtils::RecordEntry *re, char** out) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(*out);
	*out += 1;
	char* args = re->argData;
	int arg0 = *reinterpret_cast<int*>(args); args += sizeof(int);

	nib[0].first = BufferUtils::pack(out, arg0);
}

inline void
compressArgs19(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs20(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs21(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs22(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs23(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
compressArgs24(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

// Decompression Code
inline void
decompressPrintArg1(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "Messages in the Header File";
	const char *filename = "folder/Sample.h";
	const int linenum = 29;

	printf(fmtString);
}

inline void
decompressPrintArg2(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "Simple times";
	const char *filename = "simpleTest.cc";
	const int linenum = 25;

	printf(fmtString);
}

inline void
decompressPrintArg3(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 2;

	int arg0 = BufferUtils::unpack<int>(&in, nib[0].first);
	int arg1 = BufferUtils::unpack<int>(&in, nib[0].second);
	double arg2 = BufferUtils::unpack<double>(&in, nib[1].first);

	const char* arg3 = in; in += strlen(arg3) + 1;

	const char *fmtString = "Hello world number %d of %d (%0.2lf%%)! This is %s!";
	const char *filename = "simpleTest.cc";
	const int linenum = 30;

	printf(fmtString, arg0, arg1, arg2, arg3);
}

inline void
decompressPrintArg4(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "RAMCLOUD_LOG() \"RAMCLOUD_LOG(ERROR, \"Hi \")\"";
	const char *filename = "simpleTest.cc";
	const int linenum = 38;

	printf(fmtString);
}

inline void
decompressPrintArg5(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "SDF";
	const char *filename = "simpleTest.cc";
	const int linenum = 48;

	printf(fmtString);
}

inline void
decompressPrintArg6(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;


	const char* arg0 = in; in += strlen(arg0) + 1;

	const char *fmtString = "NEWLinesSoEvil %s";
	const char *filename = "simpleTest.cc";
	const int linenum = 51;

	printf(fmtString, arg0);
}

inline void
decompressPrintArg7(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "Yup\nie";
	const char *filename = "simpleTest.cc";
	const int linenum = 59;

	printf(fmtString);
}

inline void
decompressPrintArg8(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 1;

	int arg0 = BufferUtils::unpack<int>(&in, nib[0].first);


	const char *fmtString = "Hello %d";
	const char *filename = "simpleTest.cc";
	const int linenum = 65;

	printf(fmtString, arg0);
}

inline void
decompressPrintArg9(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 1;

	int arg0 = BufferUtils::unpack<int>(&in, nib[0].first);


	const char *fmtString = "This should not be incremented twice (=1):%id";
	const char *filename = "simpleTest.cc";
	const int linenum = 80;

	printf(fmtString, arg0);
}

inline void
decompressPrintArg10(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "Hello /* uncool */";
	const char *filename = "simpleTest.cc";
	const int linenum = 84;

	printf(fmtString);
}

inline void
decompressPrintArg11(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "This is rediculous";
	const char *filename = "simpleTest.cc";
	const int linenum = 88;

	printf(fmtString);
}

inline void
decompressPrintArg12(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "OLO_SWAG";
	const char *filename = "simpleTest.cc";
	const int linenum = 79;

	printf(fmtString);
}

inline void
decompressPrintArg13(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "ssneaky #define LOG";
	const char *filename = "simpleTest.cc";
	const int linenum = 97;

	printf(fmtString);
}

inline void
decompressPrintArg14(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;


	const char* arg0 = in; in += strlen(arg0) + 1;

	const char *fmtString = "No %s";
	const char *filename = "simpleTest.cc";
	const int linenum = 103;

	printf(fmtString, arg0);
}

inline void
decompressPrintArg15(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "I am so evil";
	const char *filename = "simpleTest.cc";
	const int linenum = 104;

	printf(fmtString);
}

inline void
decompressPrintArg16(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;


	const char* arg0 = in; in += strlen(arg0) + 1;

	const char *fmtString = "%s";
	const char *filename = "simpleTest.cc";
	const int linenum = 111;

	printf(fmtString, arg0);
}

inline void
decompressPrintArg17(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;


	const char* arg0 = in; in += strlen(arg0) + 1;

	const char *fmtString = "NonConst: %s";
	const char *filename = "simpleTest.cc";
	const int linenum = 116;

	printf(fmtString, arg0);
}

inline void
decompressPrintArg18(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 1;

	int arg0 = BufferUtils::unpack<int>(&in, nib[0].first);


	const char *fmtString = "{{\"(( False curlies and brackets! %d";
	const char *filename = "simpleTest.cc";
	const int linenum = 123;

	printf(fmtString, arg0);
}

inline void
decompressPrintArg19(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "Same line, bad form";
	const char *filename = "simpleTest.cc";
	const int linenum = 127;

	printf(fmtString);
}

inline void
decompressPrintArg20(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "Really bad";
	const char *filename = "simpleTest.cc";
	const int linenum = 127;

	printf(fmtString);
}

inline void
decompressPrintArg21(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "Ending on different lines";
	const char *filename = "simpleTest.cc";
	const int linenum = 129;

	printf(fmtString);
}

inline void
decompressPrintArg22(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "Make sure that the inserted code is before the ++i";
	const char *filename = "simpleTest.cc";
	const int linenum = 135;

	printf(fmtString);
}

inline void
decompressPrintArg23(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "The worse";
	const char *filename = "simpleTest.cc";
	const int linenum = 138;

	printf(fmtString);
}

inline void
decompressPrintArg24(char* &in) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(in);
	in += 0;



	const char *fmtString = "TEST";
	const char *filename = "simpleTest.cc";
	const int linenum = 143;

	printf(fmtString);
}

void (*compressFnArray[25])(BufferUtils::RecordEntry *re, char** out) {
	nullptr,
	compressArgs1,
	compressArgs2,
	compressArgs3,
	compressArgs4,
	compressArgs5,
	compressArgs6,
	compressArgs7,
	compressArgs8,
	compressArgs9,
	compressArgs10,
	compressArgs11,
	compressArgs12,
	compressArgs13,
	compressArgs14,
	compressArgs15,
	compressArgs16,
	compressArgs17,
	compressArgs18,
	compressArgs19,
	compressArgs20,
	compressArgs21,
	compressArgs22,
	compressArgs23,
	compressArgs24
};

void (*decompressAndPrintFnArray[25])(char* &in) {
	nullptr,
	decompressPrintArg1,
	decompressPrintArg2,
	decompressPrintArg3,
	decompressPrintArg4,
	decompressPrintArg5,
	decompressPrintArg6,
	decompressPrintArg7,
	decompressPrintArg8,
	decompressPrintArg9,
	decompressPrintArg10,
	decompressPrintArg11,
	decompressPrintArg12,
	decompressPrintArg13,
	decompressPrintArg14,
	decompressPrintArg15,
	decompressPrintArg16,
	decompressPrintArg17,
	decompressPrintArg18,
	decompressPrintArg19,
	decompressPrintArg20,
	decompressPrintArg21,
	decompressPrintArg22,
	decompressPrintArg23,
	decompressPrintArg24
};
// Format Id to original Format String
const char* fmtId2Str[24] = {
	"Messages in the Header File",
	"Simple times",
	"Hello world number %d of %d (%0.2lf%%)! This is %s!",
	"RAMCLOUD_LOG() \"RAMCLOUD_LOG(ERROR, \"Hi \")\"",
	"SDF",
	"NEWLinesSoEvil %s",
	"Yup\nie",
	"Hello %d",
	"This should not be incremented twice (=1):%id",
	"Hello /* uncool */",
	"This is rediculous",
	"OLO_SWAG",
	"ssneaky #define LOG",
	"No %s",
	"I am so evil",
	"%s",
	"NonConst: %s",
	"{{\"(( False curlies and brackets! %d",
	"Same line, bad form",
	"Really bad",
	"Ending on different lines",
	"Make sure that the inserted code is before the ++i",
	"The worse",
	"TEST"
};



#endif /* BUFFER_STUFFER */

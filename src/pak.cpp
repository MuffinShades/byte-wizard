/**
 *
 * pak.cpp
 * 
 * Encoding and decoding file for the MPAK format
 * 
 * Copyright muffinshades 2024
 * 
 * This file is used to decode MPAK files with the extension
 * pak and mpak. This file format was created by muffinshades.
 * Uses zlib compression
 * 
 */

#include "pch.h"
#include "pak.h"
#include "Compress.h"
#include "FileWriter.h"
#include "BinUtility.h"
#include <chrono>

template<typename _T> void CpyToBuffer(_T* buffer, _T* val, int valLen, int& bufferStart) {
	memcpy(buffer+bufferStart, val, valLen);
	bufferStart += valLen;
}

template<typename _T> void ReadFromBuffer(_T* outputBuffer, _T* buffer, int& bufferStart, size_t nBytes) {
	if (outputBuffer != nullptr && buffer != nullptr && bufferStart >= 0 && nBytes > 0) {
		memcpy(outputBuffer, buffer + bufferStart, nBytes);
		bufferStart += nBytes;
	}
}

template<class _T> unsigned char* GetValBytes(_T val) {
	unsigned char* _bytes = new unsigned char[sizeof(_T)];

	for (size_t i = 0; i < sizeof(_T); i++) {
		*(_bytes + i) = (val >> (i * 8)) & 0xff;
	}

	return _bytes;
}

unsigned char IntToUChar(int v) {
	unsigned char* tempBytes = GetValBytes(v);
	unsigned char res = *tempBytes;
	delete[] tempBytes;
	return res;
}

template<typename _T> _T AssembleBytes(unsigned char* bytes, int len) {
	if (len > 0 && bytes != nullptr) {
		_T res = 0x00;

		for (size_t i = 0; i < sizeof(_T); i++) {
			res |= (*(bytes+i) << (i * 8));
		}

		return res;
	}
	else {
		return (_T)NULL;
	}
}

//write to .pak file
int pak::PackToFile(std::string src, PakStructure data) {
	//src check
	if (src == "") {
		return PAK_ERROR_BLANK_SRC;
	}

	//load in file
	std::fstream _targetFile;
	_targetFile = std::fstream(src, std::ios::out | std::ios::binary);

	/*if (!_targetFile.good()) {
		_targetFile.close();
		return PAK_ERROR_INVALID_SRC;
	}*/

	//create pak header

	//4 bytes -> MPAK
	//2 bytes -> Number of files
	//11 bytes -> Date (includes milliseconds)
	// 
	//17 more bytes

	int allocLen = 19 + data.addtionalHeaderLen;

	unsigned char* pak_header = new unsigned char[allocLen];

	int cByte = 0; //start at 4 to leave room for header length

	int headerLen = (allocLen - 8) + data.addtionalHeaderLen; //header length -> allocated length - 8

	//to compute extra header length: header length - 13

	CpyToBuffer<unsigned char>(pak_header, (unsigned char*)"MPAK", 4, cByte);

	cByte += 4; //add 4 bytes to leave room for header len
	
	//conver the number of files into 2 bytes of data
	unsigned char* nFileBytes = GetValBytes((unsigned int)data.files.size());

	//copy over number of files
	CpyToBuffer<unsigned char>(pak_header, nFileBytes, 2, cByte);
	delete[] nFileBytes;

	//get date info

	//date struct
	struct _Date {
		unsigned short ms = 0;
		unsigned char s = 0;
		unsigned char hr = 0;
		unsigned char min = 0;
		unsigned char day = 0;
		unsigned char month = 0;
		unsigned short year = 0;
	};

	//get time
	auto _now = std::chrono::system_clock::now();
	time_t _time = std::chrono::system_clock::to_time_t(_now);
	tm local_time = *localtime(&_time);

	//construct the data
	_Date cur_date;

	cur_date.s = local_time.tm_sec;
	cur_date.min = local_time.tm_min;
	cur_date.hr = local_time.tm_hour;
	cur_date.day = local_time.tm_mday;
	cur_date.month = local_time.tm_mon+1;
	cur_date.year = local_time.tm_year+1900;

	std::cout << (int)cur_date.month << "/" << (int)cur_date.day << "/" << (int)cur_date.year << "   " << (int)cur_date.hr << ":" << (int)cur_date.min << ":" << (int)cur_date.s << std::endl;

	//add date info
	unsigned char* tempBytes = GetValBytes(cur_date.ms);
	CpyToBuffer<unsigned char>(pak_header, tempBytes, sizeof(cur_date.ms), cByte);
	
	tempBytes = GetValBytes(cur_date.s);
	CpyToBuffer<unsigned char>(pak_header, tempBytes, sizeof(cur_date.s), cByte);
	delete[] tempBytes;

	tempBytes = GetValBytes(cur_date.min);
	CpyToBuffer<unsigned char>(pak_header, tempBytes, sizeof(cur_date.min), cByte);
	delete[] tempBytes;

	tempBytes = GetValBytes(cur_date.hr);
	CpyToBuffer<unsigned char>(pak_header, tempBytes, sizeof(cur_date.hr), cByte);
	delete[] tempBytes;

	tempBytes = GetValBytes(cur_date.day);
	CpyToBuffer<unsigned char>(pak_header, tempBytes, sizeof(cur_date.day), cByte);
	delete[] tempBytes;

	tempBytes = GetValBytes(cur_date.month);
	CpyToBuffer<unsigned char>(pak_header, tempBytes, sizeof(cur_date.month), cByte);
	delete[] tempBytes;

	tempBytes = GetValBytes(cur_date.year);
	CpyToBuffer<unsigned char>(pak_header, tempBytes, sizeof(cur_date.year), cByte);
	delete[] tempBytes;

	//copy over other header info
	if (data.rawHeader != nullptr) {
		CpyToBuffer<unsigned char>(pak_header, data.rawHeader, sizeof(unsigned char)* data.rawHeaderLen, cByte);
	}
	else {
		if (data.rawHeaderLen > 0) {
			unsigned char* nullAdHeader = (unsigned char*)malloc(sizeof(unsigned char) * data.rawHeaderLen);
			CpyToBuffer<unsigned char>(pak_header, nullAdHeader, sizeof(unsigned char)* data.rawHeaderLen, cByte);
			free(nullAdHeader);
		}
	}

	//add header len
	tempBytes = GetValBytes(allocLen);
	int __start = 4;
	CpyToBuffer<unsigned char>(pak_header, tempBytes, 4, __start);
	delete[] tempBytes;

	//copy over file header
	_targetFile.write((char*)pak_header, allocLen);

	std::cout << "Pak Header len: " << allocLen << std::endl;
	std::cout << "Writing to Pak loc: " << src << std::endl;

	//header is complete now pak individual files
	long summativeSize = 0;

	for (int i = 0; i < data.files.size(); i++) {
		summativeSize += data.files[i].fSize;

		std::cout << "fSize: " << data.files[i].fSize << std::endl;

		//compute header length
		int hLen = 0;

		if (data.files[i].fName.length() > 255) {
			data.files[i].fName = data.files[i].fName.substr(0, 255);
		}

		if (data.files[i].fType.length() > 255) {
			data.files[i].fType = data.files[i].fType.substr(0, 255);
		}

		//
		hLen += 4; //header length
		hLen += 2; //length of file index
		hLen += 1; //length of file name
		hLen += data.files[i].fName.length(); //file name
		hLen += 1; //length of extension
		hLen += data.files[i].fType.length(); //file extension
		hLen += 1; //compress type (as a byte)
		hLen += sizeof(int); //aditional header length
		hLen += data.files[i].adhiLen; //additional header
		hLen += 8; //compressed len
			
		//construct header
		unsigned char* fHeader = (unsigned char*) malloc((size_t) hLen);

		//byte position
		int cByte = 0;

		//TODO add info

		//copy over header length
		unsigned char* hlenbytes = GetValBytes(hLen);
		CpyToBuffer<unsigned char>(fHeader, hlenbytes, sizeof(int), cByte);
		delete[] hlenbytes;

		//copy over file index
		unsigned char* index_bytes = GetValBytes((unsigned short) i);
		CpyToBuffer<unsigned char>(fHeader, index_bytes, sizeof(unsigned short), cByte);
		delete[] index_bytes;

		//copy over name info
		int name_len = data.files[i].fName.length();
		unsigned char* _intTempBytes = GetValBytes(name_len);
		CpyToBuffer<unsigned char>(fHeader, &_intTempBytes[0], 1, cByte);
		CpyToBuffer<unsigned char>(fHeader, (unsigned char*)data.files[i].fName.c_str(), name_len, cByte);
		delete[] _intTempBytes;


		//copy over extensino info

		_intTempBytes = GetValBytes(data.files[i].fType.length());
		CpyToBuffer<unsigned char>(fHeader, &_intTempBytes[0], sizeof(unsigned char), cByte);
		CpyToBuffer<unsigned char>(fHeader, (unsigned char*) data.files[i].fType.c_str(), data.files[i].fType.length(), cByte);
		delete[] _intTempBytes;

		//copy over compression type
		_intTempBytes = GetValBytes(data.files[i].compressType);
		CpyToBuffer<unsigned char>(fHeader, &_intTempBytes[0], sizeof(unsigned char), cByte);
		delete[] _intTempBytes;

		//copy over other header info
		unsigned char* adh_len_bytes = GetValBytes((unsigned int)data.files[i].adhiLen);
		CpyToBuffer<unsigned char>(fHeader, (unsigned char*)adh_len_bytes, sizeof(int), cByte); //copy len
		delete[] adh_len_bytes;
		CpyToBuffer<unsigned char>(fHeader, (unsigned char*)data.files[i].addHeaderInf, data.files[i].adhiLen * sizeof(unsigned char), cByte); //copy bytes

		//compress the file based off of it's compress info
		unsigned char* compressedBytes;
		int64_t compressLen = 0;

		switch (data.files[i].compressType) {

			//compress using huffman
			case COMPRESSION_HUFFMAN: {
				HuffmanResult _huffCompRes = Huffman::Encode(
					data.files[i].fData, 
					data.files[i].fSize
				);

				if (_huffCompRes.bytes != nullptr && _huffCompRes.len > 0) {
					RawBytes fByteData = _huffCompRes.GetFullByteResult();

					compressLen = fByteData.len;
					compressedBytes = new unsigned char[fByteData.len];
					memcpy(compressedBytes, fByteData.bytes, compressLen);

					delete[] fByteData.bytes;
				}
				else {
					compressLen = data.files[i].fSize;
					compressedBytes = new unsigned char[compressLen];
					memcpy(compressedBytes, data.files[i].fData, compressLen);
				}

				if (_huffCompRes.bytes != nullptr) 
					delete[] _huffCompRes.bytes;

				break;
			}
			case COMPRESSION_LZSS: {
				lzss_settings compressSettings;

				compressSettings.BYTES_PER_BUFFER = data.files[i].compressionSettings.BYTES_PER_BUFFER;
				compressSettings.b_buffer = data.files[i].compressionSettings.b_buffer;
				compressSettings.s_buffer = data.files[i].compressionSettings.s_buffer;

				lzss_result _compRes = lzss::compress(data.files[i].fData, data.files[i].fSize, compressSettings);

				if (_compRes.bytes != nullptr && _compRes.len > 0 && (_compRes.comp_ratio > 1.0f || data.files[i].compressionSettings.alwaysCompress)) {
					compressLen = _compRes.len;
					compressedBytes = new unsigned char[_compRes.len];
					memcpy(compressedBytes, _compRes.bytes, compressLen);
				}
				else {
					compressLen = data.files[i].fSize;
					compressedBytes = new unsigned char[compressLen];
					memcpy(compressedBytes, data.files[i].fData, compressLen);
				}

				if (_compRes.bytes != nullptr) delete[] _compRes.bytes;

				break;
			}
			case COMPRESSION_MZIP: {

				RawBytes _mzipRes = mzip::Encode(data.files[i].fData, data.files[i].fSize, data.files[i].compressionSettings);

				if (_mzipRes.len > 0 && _mzipRes.bytes != nullptr) {
					compressLen = _mzipRes.len;
					compressedBytes = new unsigned char[_mzipRes.len];
					memcpy(compressedBytes, _mzipRes.bytes, compressLen);
				}
				else {
					compressLen = data.files[i].fSize;
					compressedBytes = new unsigned char[compressLen];
					memcpy(compressedBytes, data.files[i].fData, compressLen);
				}

				if (_mzipRes.bytes != nullptr) delete[] _mzipRes.bytes;

				break;
			}
			default: {

				//copy over with no compression
				compressLen = data.files[i].fSize;
				compressedBytes = new unsigned char[compressLen];
				memcpy(compressedBytes, data.files[i].fData, compressLen);

				break;
			}
		}

		//add len of data
		unsigned char* _clenBytes = GetValBytes(compressLen);
		CpyToBuffer<unsigned char>(fHeader, _clenBytes, sizeof(int64_t), cByte); //copy bytes
		delete[] _clenBytes;

		//write file header to file
		_targetFile.write((char*)fHeader, hLen);

		//write over file bytes
		if (compressedBytes != nullptr) _targetFile.write((char*)compressedBytes, compressLen);
	}

	_targetFile.close();

	return PAK_NOERROR;
}

//oh boy
PakStructure pak::ExtractContents(std::string src) {
	//src check
	if (src == "") {
		PakStructure _p;
		return _p;
	}

	//load in file
	std::fstream _targetFile;
	_targetFile = std::fstream(src, std::ios::in | std::ios::binary);

	//extract some file info
	_targetFile.seekg(0, std::ios::end);
	int fileLen = _targetFile.tellg();
	_targetFile.seekg(0, std::ios::beg);

	if (fileLen < 4) {
		PakStructure _p;
		return _p;
	}

	char* sigBytes = new char[4]; //length of MPAK is 4
	_targetFile.read(sigBytes, 4);

	char* __sig = (char*)"MPAK";

	for (size_t i = 0; i < 4*sizeof(char); i += sizeof(char)) {
		if (*(sigBytes+i) != *(__sig+i)) {
			std::cout << "Pak Err 1" << std::endl;
			PakStructure _p;
			return _p;
		}
	}

	delete[] sigBytes;

	_targetFile.seekg(4);

	//first extract header
	char* _lenBytes = new char[4];
	_targetFile.read(_lenBytes, 4);

	//get header length
	unsigned int headerLen = AssembleBytes<unsigned int>((unsigned char*)_lenBytes, sizeof(int));

	delete[] _lenBytes;

	//now extract header
	if (headerLen <= 0) {
		std::cout << "Pak Err 2" << std::endl;
		PakStructure _p;
		return _p;
	}

	char* _header = new char[headerLen];
	
	_targetFile.seekg(0, std::ios::beg);
	_targetFile.read(_header, headerLen);

	if (_header == nullptr) {
		std::cout << "Pak Err 3" << std::endl;
		PakStructure _p;
		return _p;
	}

	PakStructure res;

	//copy over header
	res.rawHeader = new unsigned char[headerLen];
	memcpy(res.rawHeader, (unsigned char*)_header, headerLen);
	res.rawHeaderLen = headerLen;

	//now start to parse le header
	res.totalByteSize = fileLen;

	int nFiles = AssembleBytes<unsigned int>((unsigned char*)(_header + 8), sizeof(unsigned short));

	//copy over additional header
	int adHeaderLen = headerLen - 17;

	if (adHeaderLen > 0) {
		res.addtionalHeaderLen = adHeaderLen;
		res.addtionalHeader = new unsigned char[adHeaderLen];

		memcpy(res.addtionalHeader, (_header + 17), adHeaderLen);
	}

	//now load in files
	_targetFile.seekg(headerLen);

	int _readByteStart = _targetFile.tellg();

	for (int i = 0; i < nFiles; i++) {
		PakChunk _file;

		//load in file header
		char* tempReadBytes = new char[4];

		_targetFile.read(tempReadBytes, 4);
		int fileHeaderLen = AssembleBytes<int>((unsigned char*)tempReadBytes, sizeof(int));

		delete[] tempReadBytes;

		_targetFile.seekg(_readByteStart);

		//copy over header
		char* _fileHeader = new char[fileHeaderLen];
		_targetFile.read(_fileHeader, sizeof(char) * fileHeaderLen);

		//extract file length
		tempReadBytes = new char[sizeof(int64_t)];
		int startRead = fileHeaderLen - sizeof(int64_t);
		memcpy(tempReadBytes, _fileHeader + startRead, sizeof(int64_t));
		int64_t fileLen = AssembleBytes<int64_t>((unsigned char*)tempReadBytes, sizeof(int64_t));
		delete[] tempReadBytes;

		_file.fSize = fileLen;

		/*for (int i = 0; i < headerLen; i++) {
			
			std::cout << std::hex << (int)*(_fileHeader + i) << " ";

			if (i % 16 == 0 && i != 0) {
				std::cout << std::endl;
			}
		}

		std::cout << std::dec << std::endl;
		std::cout << std::endl;*/

		//now parse le header
		//extract more stuff from file header

		//index (2 bytes)
		//1 bytes file name len
		//file name (n bytes)
		//extension
		//1 bytes compression
		//other header info

		int headerIdx = sizeof(int);

		//file index
		tempReadBytes = new char[sizeof(short)];
		ReadFromBuffer(tempReadBytes, _fileHeader, headerIdx, sizeof(short));
		_file._fileIndex = AssembleBytes<int>((unsigned char*)tempReadBytes, sizeof(short));
		delete[] tempReadBytes;

		//file name
		int _fNameLen = (int) * (_fileHeader + headerIdx);
		headerIdx++;

		if (_fNameLen > 0) {
			char* _c_fName = new char[_fNameLen];
			ReadFromBuffer(_c_fName, _fileHeader, headerIdx, sizeof(char) * _fNameLen);
			_file.fName = std::string(_c_fName, _fNameLen);
		}

		//file extension
		char _fExtLen = *(_fileHeader + headerIdx);
		headerIdx++;

		if (_fExtLen > 0) {
			char* _c_fExt = new char[_fExtLen];
			ReadFromBuffer(_c_fExt, _fileHeader, headerIdx, sizeof(char) * _fExtLen);
			_file.fType = std::string(_c_fExt, _fExtLen);
		}

		//compression type
		char _fCompression = *(_fileHeader + headerIdx);
		headerIdx++;
		_file.compressType = (int)_fCompression;

		//other header info
		tempReadBytes = new char[sizeof(int)];
		ReadFromBuffer(tempReadBytes, _fileHeader, headerIdx, sizeof(int));
		_file.adhiLen = AssembleBytes<int>((unsigned char*) tempReadBytes, sizeof(int));
		delete[] tempReadBytes;

		if (_file.adhiLen > 0) {
			_file.addHeaderInf = new unsigned char[_file.adhiLen];
			ReadFromBuffer(_file.addHeaderInf, (unsigned char*)_fileHeader, headerIdx, _file.adhiLen * sizeof(unsigned char));
		}

		//now to extract file contents, fun!
		char* _ucFDat = new char[_file.fSize];
		_targetFile.seekg(_readByteStart + fileHeaderLen);
		_targetFile.read(_ucFDat, _file.fSize);

		int _fpSize = _file.fSize;

		//now uncompress even funner!
		switch (_file.compressType) {
			case COMPRESSION_HUFFMAN: {
				HuffmanResult _hcRes = Huffman::Decode(
					(unsigned char*) _ucFDat,
					_file.fSize
				);

				if (_hcRes.bytes != nullptr && _hcRes.len > 0) {
					_file.fSize = _hcRes.len;
					_file.fData = new unsigned char[_file.fSize];
					memcpy(_file.fData, _hcRes.bytes, _file.fSize);
					delete[] _hcRes.bytes;
					delete[] _ucFDat;
				}
				else {
					_file.fData = new unsigned char[_file.fSize];
					memcpy(_file.fData, _ucFDat, _file.fSize);
					delete[] _ucFDat;
				}

				break;
			}

			//TODO add settings
			case COMPRESSION_LZSS: {
				lzss_result _lcRes = lzss::decompress(
					(unsigned char*)_ucFDat,
					_file.fSize
				);

				if (_lcRes.bytes != nullptr && _lcRes.len > 0) {
					_file.fSize = _lcRes.len;
					_file.fData = new unsigned char[_file.fSize];
					memcpy(_file.fData, _lcRes.bytes, _file.fSize);
					delete[] _lcRes.bytes;
					delete[] _ucFDat;
				}
				else {
					_file.fData = new unsigned char[_file.fSize];
					memcpy(_file.fData, _ucFDat, _file.fSize);
					delete[] _ucFDat;
				}

				break;
			}

			case COMPRESSION_MZIP: {
				RawBytes _mcRes = mzip::Decode(
					(unsigned char*)_ucFDat,
					_file.fSize
				);

				if (_mcRes.bytes != nullptr && _mcRes.len > 0) {
					_file.fSize = _mcRes.len;
					_file.fData = new unsigned char[_file.fSize];
					memcpy(_file.fData, _mcRes.bytes, _file.fSize);
					delete[] _mcRes.bytes;
					delete[] _ucFDat;
				}
				else {
					_file.fData = new unsigned char[_file.fSize];
					memcpy(_file.fData, _ucFDat, _file.fSize);
					delete[] _ucFDat;
				}

				break;
			}

			default: {
				_file.fData = new unsigned char[_file.fSize];
				memcpy(_file.fData, _ucFDat, _file.fSize);
				delete[] _ucFDat;

				break;
			}
		}

		delete[] _fileHeader;

		PakChunk _tempFile;

		res.files.push_back(_tempFile);

		int __idx = res.files.size() - 1;

		//copy over to file in result
		res.files[__idx].fSize = _file.fSize;
		res.files[__idx].fData = new unsigned char[_file.fSize];
		memcpy(res.files[__idx].fData, _file.fData, _file.fSize);
		delete[] _file.fData;
		res.files[__idx].adhiLen = _file.adhiLen;
		res.files[__idx].addHeaderInf = new unsigned char[_file.adhiLen];
		memcpy(res.files[__idx].addHeaderInf, _file.addHeaderInf, _file.adhiLen);
		delete[] _file.addHeaderInf;
		res.files[__idx].fName = _file.fName;
		res.files[__idx].fType = _file.fType;
		res.files[__idx].compressType = _file.compressType;
		res.files[__idx]._fileIndex = _file._fileIndex;

		//go to next file
		_readByteStart = _readByteStart + fileHeaderLen + _fpSize;
		_targetFile.seekg(_readByteStart);
	}

	delete[] _header;
	_targetFile.close();
	return res;
}

int pak::ExtractContentsToDisk(std::string pakSrc, std::string resSrc) {
	if (pakSrc != "" && resSrc != "") {
		
		//extract the structure
		PakStructure t_res = pak::ExtractContents(pakSrc);

		//now unpak to directory
		for (int i = 0; i < t_res.files.size(); i++) {
			if (t_res.files[i].fSize > 0 && t_res.files[i].fData != nullptr) {
				std::fstream _fWrite;
				//std::cout << "file " << i << " src: " << resSrc + t_res.files[i].fName + t_res.files[i].fType << std::endl;
				_fWrite.open(resSrc + t_res.files[i].fName + t_res.files[i].fType, std::ios::out | std::ios::binary);
				_fWrite.write((char*)t_res.files[i].fData, t_res.files[i].fSize);
				_fWrite.close();
			}
		}

		//finally unpack header

		return 0x00;
	}
	else {
		return PAK_ERROR_BLANK_SRC;
	}
}

#include <filesystem>
#include <sys/stat.h>

namespace fs = std::filesystem;

int pak::PakFilesInDirectory(std::string targetDir, std::string outputSrc, int compressionType, MZIP_SETTINGS compressionSettings) {
	if (targetDir != "" && outputSrc != "") {
		struct stat sb;

		PakStructure _pks;
		int i = 0;
		for (const auto& _e : fs::directory_iterator(targetDir)) {
			std::filesystem::path _file_name = _e.path();
			std::string _f_name = _file_name.string();
			i++;

			const char* _path = (char*)_file_name.c_str();

			//verify it is a file
			if (!(stat(_path, &sb) == 0 && !(sb.st_mode & S_IFDIR))) {

				//now add file to pak stucture
				std::fstream _file;
				_file.open(_f_name, std::ios::binary | std::ios::in);

				PakChunk _pc;

				_file.seekg(0, std::ios::end);
				_pc.fSize = _file.tellg();
				_file.seekg(0, std::ios::beg);
				
				_pc.fData = new unsigned char[_pc.fSize];
				char* tempBytes = new char[_pc.fSize];
				_file.read(tempBytes, _pc.fSize);
				memcpy(_pc.fData, tempBytes, _pc.fSize);
				delete[] tempBytes;

				_pc.compressType = compressionType;
				_pc.compressionSettings = compressionSettings;
				_pc.fName = _file_name.filename().string();
				_pc.fType = _file_name.extension().string();

				_pks.files.push_back(_pc);
			}
		}

		//finally pack files
		pak::PackToFile(outputSrc, _pks);

		return 0;
	}
	else {
		return PAK_ERROR_BLANK_SRC;
	}
}

PakChunk pak::ExtractFile(std::string src, PakInfo file) {
	if (src != "" && (file.fileName != "" || file.nFile >= 0)) {
		//load in file
		std::fstream _targetFile;
		_targetFile = std::fstream(src, std::ios::in | std::ios::binary);

		//extract some file info
		_targetFile.seekg(0, std::ios::end);
		int fileLen = _targetFile.tellg();
		_targetFile.seekg(0, std::ios::beg);

		if (fileLen < 4) {
			PakChunk _p;
			return _p;
		}

		char* sigBytes = new char[4]; //length of MPAK is 4
		_targetFile.read(sigBytes, 4);

		char* __sig = (char*)"MPAK";

		for (size_t i = 0; i < 4 * sizeof(char); i += sizeof(char)) {
			if (*(sigBytes + i) != *(__sig + i)) {
				std::cout << "Pak Err 1" << std::endl;
				PakChunk _p;
				return _p;
			}
		}

		delete[] sigBytes;

		_targetFile.seekg(4);

		//first extract header
		char* _lenBytes = new char[4];
		_targetFile.read(_lenBytes, 4);

		//get header length
		unsigned int headerLen = AssembleBytes<unsigned int>((unsigned char*)_lenBytes, sizeof(int));

		delete[] _lenBytes;

		//now extract header
		if (headerLen <= 0) {
			std::cout << "Pak Err 2" << std::endl;
			PakChunk _p;
			return _p;
		}

		char* _header = new char[headerLen];

		_targetFile.seekg(0, std::ios::beg);
		_targetFile.read(_header, headerLen);

		if (_header == nullptr) {
			std::cout << "Pak Err 3" << std::endl;
			PakChunk _p;
			return _p;
		}

		int nFiles = AssembleBytes<unsigned int>((unsigned char*)(_header + 8), sizeof(unsigned short));

		//now load in files
		_targetFile.seekg(headerLen);

		int _readByteStart = _targetFile.tellg();

		for (int i = 0; i < nFiles; i++) {

			PakChunk _file;

			//load in file header
			char* tempReadBytes = new char[4];

			_targetFile.read(tempReadBytes, 4);
			int fileHeaderLen = AssembleBytes<int>((unsigned char*)tempReadBytes, sizeof(int));

			delete[] tempReadBytes;

			_targetFile.seekg(_readByteStart);

			//copy over header
			char* _fileHeader = new char[fileHeaderLen];
			_targetFile.read(_fileHeader, sizeof(char) * fileHeaderLen);

			//extract file length
			tempReadBytes = new char[sizeof(int64_t)];
			int startRead = fileHeaderLen - sizeof(int64_t);
			memcpy(tempReadBytes, _fileHeader + startRead, sizeof(int64_t));
			int64_t fileLen = AssembleBytes<int64_t>((unsigned char*)tempReadBytes, sizeof(int64_t));
			delete[] tempReadBytes;

			_file.fSize = fileLen;

			/*for (int i = 0; i < headerLen; i++) {

				std::cout << std::hex << (int)*(_fileHeader + i) << " ";

				if (i % 16 == 0 && i != 0) {
					std::cout << std::endl;
				}
			}

			std::cout << std::dec << std::endl;
			std::cout << std::endl;*/

			//now parse le header
			//extract more stuff from file header

			//index (2 bytes)
			//1 bytes file name len
			//file name (n bytes)
			//extension
			//1 bytes compression
			//other header info

			int headerIdx = sizeof(int);

			//file index
			tempReadBytes = new char[sizeof(short)];
			ReadFromBuffer(tempReadBytes, _fileHeader, headerIdx, sizeof(short));
			_file._fileIndex = AssembleBytes<int>((unsigned char*)tempReadBytes, sizeof(short));
			delete[] tempReadBytes;

			//file name
			int _fNameLen = (int)*(_fileHeader + headerIdx);
			headerIdx++;

			if (_fNameLen > 0) {
				char* _c_fName = new char[_fNameLen];
				ReadFromBuffer(_c_fName, _fileHeader, headerIdx, sizeof(char) * _fNameLen);
				_file.fName = std::string(_c_fName, _fNameLen);
			}

			//file extension
			char _fExtLen = *(_fileHeader + headerIdx);
			headerIdx++;

			if (_fExtLen > 0) {
				char* _c_fExt = new char[_fExtLen];
				ReadFromBuffer(_c_fExt, _fileHeader, headerIdx, sizeof(char) * _fExtLen);
				_file.fType = std::string(_c_fExt, _fExtLen);
			}

			bool _cFile = (file.fileName == _file.fName || file.nFile == i);

			if (_cFile) {
				//compression type
				char _fCompression = *(_fileHeader + headerIdx);
				headerIdx++;
				_file.compressType = (int)_fCompression;

				//other header info
				tempReadBytes = new char[sizeof(int)];
				ReadFromBuffer(tempReadBytes, _fileHeader, headerIdx, sizeof(int));
				_file.adhiLen = AssembleBytes<int>((unsigned char*)tempReadBytes, sizeof(int));
				delete[] tempReadBytes;

				if (_file.adhiLen > 0) {
					_file.addHeaderInf = new unsigned char[_file.adhiLen];
					ReadFromBuffer(_file.addHeaderInf, (unsigned char*)_fileHeader, headerIdx, _file.adhiLen * sizeof(unsigned char));
				}

				//now to extract file contents, fun!
				char* _ucFDat = new char[_file.fSize];
				_targetFile.seekg(_readByteStart + fileHeaderLen);
				_targetFile.read(_ucFDat, _file.fSize);

				int _fpSize = _file.fSize;

				//now uncompress even funner!
				switch (_file.compressType) {
				case COMPRESSION_HUFFMAN: {
					HuffmanResult _hcRes = Huffman::Decode(
						(unsigned char*)_ucFDat,
						_file.fSize
					);

					if (_hcRes.bytes != nullptr && _hcRes.len > 0) {
						_file.fSize = _hcRes.len;
						_file.fData = new unsigned char[_file.fSize];
						memcpy(_file.fData, _hcRes.bytes, _file.fSize);
						delete[] _hcRes.bytes;
						delete[] _ucFDat;
					}
					else {
						_file.fData = new unsigned char[_file.fSize];
						memcpy(_file.fData, _ucFDat, _file.fSize);
						delete[] _ucFDat;
					}

					break;
				}

										//TODO add settings
				case COMPRESSION_LZSS: {
					lzss_result _lcRes = lzss::decompress(
						(unsigned char*)_ucFDat,
						_file.fSize
					);

					if (_lcRes.bytes != nullptr && _lcRes.len > 0) {
						_file.fSize = _lcRes.len;
						_file.fData = new unsigned char[_file.fSize];
						memcpy(_file.fData, _lcRes.bytes, _file.fSize);
						delete[] _lcRes.bytes;
						delete[] _ucFDat;
					}
					else {
						_file.fData = new unsigned char[_file.fSize];
						memcpy(_file.fData, _ucFDat, _file.fSize);
						delete[] _ucFDat;
					}

					break;
				}

				case COMPRESSION_MZIP: {
					RawBytes _mcRes = mzip::Decode(
						(unsigned char*)_ucFDat,
						_file.fSize
					);

					if (_mcRes.bytes != nullptr && _mcRes.len > 0) {
						_file.fSize = _mcRes.len;
						_file.fData = new unsigned char[_file.fSize];
						memcpy(_file.fData, _mcRes.bytes, _file.fSize);
						delete[] _mcRes.bytes;
						delete[] _ucFDat;
					}
					else {
						_file.fData = new unsigned char[_file.fSize];
						memcpy(_file.fData, _ucFDat, _file.fSize);
						delete[] _ucFDat;
					}

					break;
				}

				default: {
					_file.fData = new unsigned char[_file.fSize];
					memcpy(_file.fData, _ucFDat, _file.fSize);
					delete[] _ucFDat;

					break;
				}
				}

				delete[] _fileHeader;

				_targetFile.close();

				return _file;
			}

			

			//go to next file
			_readByteStart = _readByteStart + fileHeaderLen + _file.fSize;
			_targetFile.seekg(_readByteStart);
		}
		
		delete[] _header;
		_targetFile.close();
		PakChunk _noChunk;
		_noChunk.resType = PAKRESULT_NOFILEFOUND;
	}
	else {
		PakChunk _nullChunk;
		return _nullChunk;
	}
}
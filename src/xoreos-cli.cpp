/* xoreos-tools - Tools to help with xoreos development
 *
 * xoreos-tools is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * xoreos-tools is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * xoreos-tools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xoreos-tools. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  Tool to extract KEY/BIF archives.
 */

#include <cstring>
#include <cstdio>
#include <list>
#include <vector>

#include "src/common/version.h"
#include "src/common/util.h"
#include "src/common/ustring.h"
#include "src/common/error.h"
#include "src/common/platform.h"
#include "src/common/readstream.h"
#include "src/common/readfile.h"
#include "src/common/filepath.h"
#include "src/common/stdoutstream.h"

#include "src/aurora/util.h"
#include "src/aurora/keyfile.h"
#include "src/aurora/biffile.h"

#include "src/util.h"

enum Command {
	kCommandNone    = -1,
	kCommandList    =  0,
	kCommandExtract     ,
	kCommandMAX
};

const char *kCommandChar[kCommandMAX] = { "list", "extract" };

void printUsage(FILE *stream, const Common::UString &name);
bool parseCommandLine(const std::vector<Common::UString> &argv, int &returnValue,
                      Command &command, Common::UString &keyFile, Common::UString& resPath,
                      Aurora::GameID &game);

uint32 getFileID(const Common::UString &fileName);
void identifyFiles(const std::list<Common::UString> &files, std::vector<Common::UString> &keyFiles,
                   std::vector<Common::UString> &bifFiles);

void openKEYs(const std::vector<Common::UString> &keyFiles, std::vector<Aurora::KEYFile *> &keys);
void openBIFs(const std::vector<Common::UString> &bifFiles, std::vector<Aurora::BIFFile *> &bifs);

void mergeKEYBIF(std::vector<Aurora::KEYFile *> &keys, std::vector<Aurora::BIFFile *> &bifs,
                 const std::vector<Common::UString> &bifFiles);

void listFiles(const Aurora::KEYFile &key, Aurora::GameID game);
void extractFile(const Aurora::KEYFile &key, const Common::UString &keyFile, const Common::UString &resPath, Aurora::GameID game);

int main(int argc, char **argv) {
	std::vector<Aurora::KEYFile *> keys;
	std::vector<Aurora::BIFFile *> bifs;

	try {
		std::vector<Common::UString> args;
		Common::Platform::getParameters(argc, argv, args);

		Aurora::GameID game = Aurora::kGameIDUnknown;

		int returnValue = 1;
		Command command = kCommandNone;
		Common::UString keyFile;
		Common::UString resPath;

		if (!parseCommandLine(args, returnValue, command, keyFile, resPath, game))
			return returnValue;

		//std::vector<Common::UString> keyFiles, bifFiles;
		//identifyFiles(files, keyFiles, bifFiles);

		Common::ReadFile stream(keyFile);
		Aurora::KEYFile key = Aurora::KEYFile(stream);
		//openKEYs(keyFile, keys);

		//openBIFs(bifFiles, bifs);

		//mergeKEYBIF(keys, bifs, bifFiles);

		if      (command == kCommandList)
			listFiles(key, game);
		else if (command == kCommandExtract)
			extractFile(key, keyFile, resPath, game);

	} catch (...) {
		for (std::vector<Aurora::KEYFile *>::iterator k = keys.begin(); k != keys.end(); ++k)
			delete *k;
		for (std::vector<Aurora::BIFFile *>::iterator b = bifs.begin(); b != bifs.end(); ++b)
			delete *b;

		Common::exceptionDispatcherError();
	}

	for (std::vector<Aurora::KEYFile *>::iterator k = keys.begin(); k != keys.end(); ++k)
		delete *k;
	for (std::vector<Aurora::BIFFile *>::iterator b = bifs.begin(); b != bifs.end(); ++b)
		delete *b;

	return 0;
}

bool parseCommandLine(const std::vector<Common::UString> &argv, int &returnValue,
                      Command &command, Common::UString &keyFile, Common::UString& resPath,
                      Aurora::GameID &game) {

	std::vector<Common::UString> args;

	bool optionsEnd = false;
	for (size_t i = 1; i < argv.size(); i++) {
		bool isOption = false;

		// A "--" marks an end to all options
		if (argv[i] == "--") {
			optionsEnd = true;
			continue;
		}

		// We're still handling options
		if (!optionsEnd) {
			// Help text
			if ((argv[i] == "-h") || (argv[i] == "--help")) {
				printUsage(stdout, argv[0]);
				returnValue = 0;

				return false;
			}

			if (argv[i] == "--version") {
				printVersion();
				returnValue = 0;

				return false;
			}

			if        (argv[i] == "--nwn2") {
				isOption = true;
				game     = Aurora::kGameIDNWN2;
			} else if (argv[i] == "--jade") {
				isOption = true;
			  game     = Aurora::kGameIDJade;
			} else if (argv[i].beginsWith("-") || argv[i].beginsWith("--")) {
			  // An options, but we already checked for all known ones

				printUsage(stderr, argv[0]);
				returnValue = 1;

				return false;
			}
		}

		// Was this a valid option? If so, don't try to use it as a file
		if (isOption)
			continue;

		args.push_back(argv[i]);
	}

	if (args.size() < 2) {
		printUsage(stderr, argv[0]);
		returnValue = 1;

		return false;
	}

	std::vector<Common::UString>::iterator arg = args.begin();

	// Find out what we should do
	command = kCommandNone;
	for (int i = 0; i < kCommandMAX; i++)
		if (!strcmp(arg->c_str(), kCommandChar[i]))
			command = (Command) i;

	// Unknown command
	if (command == kCommandNone) {
		printUsage(stderr, argv[0]);
		returnValue = 1;

		return false;
	}

	++arg;

	if (command == kCommandList) {
		keyFile = *arg;
	} else if (command == kCommandExtract) {
		keyFile = *arg++;
		resPath = *arg;
	}

	return true;
}

void printUsage(FILE *stream, const Common::UString &name) {
	std::fprintf(stream, "BioWare KEY/BIF archive extractor\n\n");
	std::fprintf(stream, "Usage: %s [<options>] <command> <file> [...]\n\n", name.c_str());
	std::fprintf(stream, "Options:\n");
	std::fprintf(stream, "  -h      --help     This help text\n");
	std::fprintf(stream, "          --version  Display version information\n");
	std::fprintf(stream, "          --nwn2     Alias file types according to Neverwinter Nights 2 rules\n");
	std::fprintf(stream, "          --jade     Alias file types according to Jade Empire rules\n\n");
	std::fprintf(stream, "Commands:\n");
	std::fprintf(stream, "  list       List files indexed in KEY archive(s)\n");
	std::fprintf(stream, "  extract    Extract BIF archive(s). Needs KEY file(s) indexing these BIF.\n\n");
	std::fprintf(stream, "Examples:\n");
	std::fprintf(stream, "%s list foo.key\n", name.c_str());
	std::fprintf(stream, "%s extract bar.key foo.bif/data/baz.2da\n", name.c_str());
}

uint32 getFileID(const Common::UString &fileName) {
	Common::ReadFile file(fileName);

	return file.readUint32BE();
}

void identifyFiles(const std::list<Common::UString> &files, std::vector<Common::UString> &keyFiles,
                   std::vector<Common::UString> &bifFiles) {

	keyFiles.reserve(files.size());
	bifFiles.reserve(files.size());

	for (std::list<Common::UString>::const_iterator f = files.begin(); f != files.end(); ++f) {
		uint32 id = getFileID(*f);

		if      (id == MKTAG('K', 'E', 'Y', ' '))
			keyFiles.push_back(*f);
		else if (id == MKTAG('B', 'I', 'F', 'F'))
			bifFiles.push_back(*f);
		else
			throw Common::Exception("File \"%s\" is neither a KEY nor a BIF", f->c_str());
	}
}

void openKEYs(const std::vector<Common::UString> &keyFiles, std::vector<Aurora::KEYFile *> &keys) {
	keys.reserve(keyFiles.size());

	for (std::vector<Common::UString>::const_iterator f = keyFiles.begin(); f != keyFiles.end(); ++f) {
		Common::ReadFile key(*f);

		keys.push_back(new Aurora::KEYFile(key));
	}
}

void openBIFs(const std::vector<Common::UString> &bifFiles, std::vector<Aurora::BIFFile *> &bifs) {
	bifs.reserve(bifFiles.size());

	for (std::vector<Common::UString>::const_iterator f = bifFiles.begin(); f != bifFiles.end(); ++f)
		bifs.push_back(new Aurora::BIFFile(new Common::ReadFile(*f)));
}

void mergeKEYBIF(std::vector<Aurora::KEYFile *> &keys, std::vector<Aurora::BIFFile *> &bifs,
                 const std::vector<Common::UString> &bifFiles) {

	// Go over all KEYs
	for (std::vector<Aurora::KEYFile *>::iterator k = keys.begin(); k != keys.end(); ++k) {

		// Go over all BIFs handled by the KEY
		const Aurora::KEYFile::BIFList &keyBifs = (*k)->getBIFs();
		for (uint kb = 0; kb < keyBifs.size(); kb++) {

			// Go over all BIFs
			for (uint b = 0; b < bifFiles.size(); b++) {

				// If they match, merge
				if (Common::FilePath::getFile(keyBifs[kb]).equalsIgnoreCase(Common::FilePath::getFile(bifFiles[b])))
					bifs[b]->mergeKEY(**k, kb);

			}

		}

	}

}

void listFiles(const Aurora::KEYFile &key, Aurora::GameID game) {
	const Aurora::KEYFile::ResourceList &resources = key.getResources();
	const Aurora::KEYFile::BIFList &bifs = key.getBIFs();

	for (Aurora::KEYFile::ResourceList::const_iterator r = resources.begin(); r != resources.end(); ++r) {
		const Aurora::FileType type     = TypeMan.aliasFileType(r->type, game);
		const Common::UString  fileName = TypeMan.setFileType(r->name, type);

		std::printf("{\"name\":\"%s\",\"type\":\"%s\",\"bif\":\"%s\"}\n", r->name.c_str(), TypeMan.setFileType("", type).c_str()+1,
		                             bifs[r->bifIndex].c_str());
	}
}

void extractFile(const Aurora::KEYFile &key, const Common::UString &keyFile, const Common::UString &resPath, Aurora::GameID game) {
	const Aurora::KEYFile::ResourceList &resources = key.getResources();
	const Aurora::KEYFile::BIFList &bifs = key.getBIFs();

	for (Aurora::KEYFile::ResourceList::const_iterator r = resources.begin(); r != resources.end(); ++r) {
		const Aurora::FileType type     = TypeMan.aliasFileType(r->type, game);
		const Common::UString  fileName = TypeMan.setFileType(r->name, type);
		const Common::UString  bifName  = bifs[r->bifIndex].c_str();

		if (bifName + "/" + fileName != resPath)
			continue;

		Common::UString dirName = keyFile;
		dirName.truncate(keyFile.size() - Common::FilePath::getFile(keyFile).size());

		Aurora::BIFFile bif = Aurora::BIFFile(new Common::ReadFile(dirName + bifName));

		Common::SeekableReadStream *stream = 0;
		try {
			stream = bif.getResource(r->resIndex);

			Common::StdOutStream sout;
			sout.writeStream(*stream);
			sout.flush();
		} catch (Common::Exception &e) {
			Common::printException(e, "");
		}

		delete stream;
	}

}

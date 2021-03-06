//------------------------------------------------------------------------------
// 文件: parser.h
// 作者: 洪坤安
// 说明: 解析当前cpp文件
//------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include "history.h"

using namespace std;
using namespace clang;

namespace clang
{
	class HeaderSearch;
	class MacroDefinition;
	class Token;
	class QualType;
	class CXXRecordDecl;
	class NamedDecl;
	class Rewriter;
	class CompilerInstance;
	class NestedNameSpecifier;
	class Type;
	class NamespaceDecl;
	class NamespaceAliasDecl;
	class UsingDirectiveDecl;
	class FunctionDecl;
	class MacroArgs;
	class VarDecl;
	class ValueDecl;
	class RecordDecl;
	class UsingDecl;
	class TemplateArgument;
	class TemplateArgumentList;
	class TemplateDecl;
	class CXXConstructorDecl;
	class DeclContext;
	class UsingShadowDecl;
	class SourceManager;
}

// [文件名] -> [路径类别：系统路径或用户路径]
typedef std::map<string, SrcMgr::CharacteristicKind> IncludeDirMap;

// class、struct、union集合
typedef std::set<const CXXRecordDecl*> RecordSet;

// using列表
typedef std::vector<const UsingShadowDecl*> UsingVec;

// [位置] -> [使用的class、struct引用或指针]
typedef std::map<SourceLocation, RecordSet> LocUseRecordsMap;

// [文件] -> [使用的class、struct引用或指针]
typedef std::map<FileID, RecordSet> FileUseRecordsMap;

// [文件] -> [该文件所使用的class、struct、union指针或引用]
typedef std::map<FileID, LocUseRecordsMap> UseRecordsByFileMap;

// 文件集
typedef std::set<FileID> FileSet;

// 文件列表
typedef std::vector<FileID> FileVec;

// 文件名集
typedef std::set<std::string> FileNameSet;

// set加上set
template <typename Container1, typename Container2>
inline void Add(Container1 &a, const Container2 &b)
{
	a.insert(b.begin(), b.end());
}

// set加上map
template <typename Key, typename Val>
inline void Add(std::set<Key> &a, const std::map<Key, Val> &b)
{
	for (const auto &itr : b)
	{
		a.insert(itr.first);
	}
}

template <typename Container, typename Key>
inline bool Has(Container& container, const Key &key)
{
	return container.find(key) != container.end();
}

// 当前正在解析的c++文件信息
class ParsingFile
{
	// 用于调试：引用的名称
	struct UseNameInfo
	{
		inline void AddName(const char* name, int line)
		{
			if (nameMap.find(name) == nameMap.end())
			{
				nameVec.push_back(name);
			}

			nameMap[name].insert(line);
		}

		FileID									file;
		std::vector<string>						nameVec;
		std::map<string, std::set<int>>			nameMap;
	};

	// 命令空间信息
	struct NamespaceInfo
	{
		NamespaceInfo()
			: ns(nullptr) {}

		std::string			name;		// 命名空间的声明，如：namespace A{ namespace B { namespace C {} } }
		const NamespaceDecl	*ns;		// 命名空间的定义
	};

public:
	// 头文件搜索路径
	struct HeaderSearchDir
	{
		HeaderSearchDir(const string& dir, SrcMgr::CharacteristicKind dirType)
			: m_dir(dir)
			, m_dirType(dirType)
		{}

		string						m_dir;
		SrcMgr::CharacteristicKind	m_dirType;
	};

public:
	ParsingFile(clang::CompilerInstance &compiler);

	~ParsingFile();

	inline clang::SourceManager& GetSrcMgr() const { return *m_srcMgr; }

	// 添加成员文件
	void AddFile(FileID file);

	// 当前cpp文件分析开始
	void Begin();

	// 当前cpp文件分析结束
	void End();

	// 分析
	void Analyze();

	// 计算文件深度
	int GetDeepth(FileID file) const;

	// 是否为可前置声明的类型
	bool IsForwardType(const QualType &var);

	// 是否所有的限定符都是命名空间（例如::std::vector<int>::中的vector就不是命名空间）
	bool IsAllQualifierNamespace(const NestedNameSpecifier *specifier);

	// a文件使用b文件
	inline void UseInclude(FileID a, FileID b, const char* name = nullptr, int line = 0);

	// 当前位置使用指定的宏
	void UseMacro(SourceLocation loc, const MacroDefinition &macro, const Token &macroName, const MacroArgs *args = nullptr);

	// 去除指针，获取变量最终指向的类型
	QualType GetPointeeType(const QualType &var);

	// 新增使用变量记录
	void UseVarType(SourceLocation loc, const QualType &var, const NestedNameSpecifier *specifier = nullptr);

	// 引用构造函数
	void UseConstructor(SourceLocation loc, const CXXConstructorDecl *constructor);

	// 引用变量声明
	void UseVarDecl(SourceLocation loc, const VarDecl *var, const NestedNameSpecifier *specifier = nullptr);

	// 引用变量声明（为左值）、函数表示、enum常量
	void UseValueDecl(SourceLocation loc, const ValueDecl *valueDecl, const NestedNameSpecifier *specifier = nullptr);

	// 引用带有名称的声明
	void UseNameDecl(SourceLocation loc, const NamedDecl *nameDecl);

	// 新增使用函数声明记录
	void UseFuncDecl(SourceLocation loc, const FunctionDecl *funcDecl);

	// 引用模板参数
	void UseTemplateArgument(SourceLocation loc, const TemplateArgument &arg);

	// 引用模板参数列表
	void UseTemplateArgumentList(SourceLocation loc, const TemplateArgumentList *args);

	// 引用模板定义
	void UseTemplateDecl(SourceLocation loc, const TemplateDecl *decl);

	// 新增使用class、struct、union记录
	void UseRecord(SourceLocation loc, const RecordDecl *record);

	// 是否为系统头文件（例如<vector>、<iostream>等就是系统文件）
	inline bool IsSystemHeader(FileID file) const;

	// 指定位置是否在系统头文件内（例如<vector>、<iostream>等就是系统文件）
	bool IsInSystemHeader(SourceLocation loc) const;

	// a位置的代码使用b位置的代码
	inline void Use(SourceLocation a, SourceLocation b, const char* name = nullptr);

	// 当前位置使用目标类型（注：QualType包含对某个类型的const、volatile、static等的修饰）
	void UseQualType(SourceLocation loc, const QualType &t, const NestedNameSpecifier *specifier = nullptr);

	// 当前位置使用目标类型（注：Type代表某个类型，但不含const、volatile、static等的修饰）
	inline void UseType(SourceLocation loc, const Type *t, const NestedNameSpecifier *specifier = nullptr);

	// 引用上下文，如命名空间
	void UseContext(SourceLocation loc, const DeclContext*);

	// 引用嵌套名字修饰符
	void UseQualifier(SourceLocation loc, const NestedNameSpecifier*);

	// 引用命名空间声明
	void UseNamespaceDecl(SourceLocation loc, const NamespaceDecl*);

	bool isBeforeInTranslationUnit(SourceLocation LHS, SourceLocation RHS) const;

	// 引用using namespace声明
	bool UseUsingNamespace(SourceLocation loc, const NamespaceDecl*, bool mustAncestor);

	// 搜寻所需的using namespace声明
	bool SearchUsingNamespace(SourceLocation loc, const NestedNameSpecifier *specifier, const DeclContext *context, bool mustAncestor);

	// 搜寻所需的using声明
	bool SearchUsingXXX(SourceLocation loc, const NestedNameSpecifier *specifier, const NamedDecl *nameDecl, bool mustAncestor);

	// 搜寻所需的using namespace或using声明
	void SearchUsingAny(SourceLocation loc, const NestedNameSpecifier *specifier, const NamedDecl *nameDecl);

	// 引用using声明
	bool UseUsing(SourceLocation loc, const NamedDecl*, bool mustAncestor);

	// 引用命名空间别名
	void UseNamespaceAliasDecl(SourceLocation loc, const NamespaceAliasDecl*);

	// 声明了命名空间
	void DeclareNamespace(const NamespaceDecl *d);

	// using了命名空间，比如：using namespace std;
	void UsingNamespace(const UsingDirectiveDecl *d);

	// using了命名空间下的某类，比如：using std::string;
	void UsingXXX(const UsingDecl *d);

	// 获取命名空间的全部路径，例如，返回namespace A{ namespace B{ class C; }}
	std::string GetNestedNamespace(const NamespaceDecl *d);

	// 当a使用b时，设法找到一个最能与a搭上关系的b的外部祖先
	inline FileID GetBestAncestor(FileID a, FileID b) const;

	// 开始清理文件（将改动c++源文件）
	void Clean();

	// 将清理结果回写到c++源文件，返回：true回写成功、false回写失败
	// （本接口参考了Rewriter::overwriteChangedFiles）
	bool Overwrite();

	// 打印索引 + 1
	std::string AddPrintIdx() const;

	// 打印信息
	void Print();

	// 获取本文件的编译错误历史
	CompileErrorHistory& GetCompileErrorHistory() { return m_compileErrorHistory; }

	// 获取指定范围的文本
	std::string GetSourceOfRange(SourceRange range) const;

	// 获取指定位置的文本
	const char* GetSourceAtLoc(SourceLocation loc) const;

	// 该文件是否是被-include强制包含
	inline bool IsForceInclude(FileID file) const;

	// 获取文件名（通过clang库接口，文件名未经过处理，可能是绝对路径，也可能是相对路径）
	// 例如：可能返回：d:/hello.h，也可能返回：./hello.h
	inline const char* GetFileName(FileID file) const;

	// 获取文件的绝对路径
	inline string GetAbsoluteFileName(FileID file) const;

	// 获取文件的绝对路径
	inline const char* GetFileNameInCache(FileID file) const;

	// 获取文件的小写绝对路径
	inline const char* GetLowerFileNameInCache(FileID file) const;

	// 用于调试：获取文件的绝对路径和相关信息
	string GetDebugFileName(FileID file) const;

	// 获取该文件的被包含信息，返回内容包括：该文件名、父文件名、被父文件#include的行号、被父文件#include的原始文本串
	std::string DebugBeIncludeText(FileID file) const;

private:
	// 获取头文件搜索路径
	std::vector<HeaderSearchDir> TakeHeaderSearchPaths(const clang::HeaderSearch &headerSearch) const;

	// 将头文件搜索路径根据长度由长到短排列
	std::vector<HeaderSearchDir> SortHeaderSearchPath(const IncludeDirMap& include_dirs_map) const;

	// 2个文件是否文件名一样
	inline bool IsSameName(FileID a, FileID b) const;

	// 获取文件的深度（令主文件的高度为0）
	int GetDepth(FileID child) const;

	// 获取指定位置所在行的文本
	std::string GetSourceOfLine(SourceLocation loc) const;

	/*
		根据传入的代码位置返回该行的正文范围（不含换行符）：[该行开头，该行末]
		例如：
			windows格式：
				int			a		=	100;\r\nint b = 0;
				^			^				^
				行首			传入的位置		行末

			linux格式：
				int			a		=	100;\n
				^			^				^
				行首			传入的位置		行末
	*/
	SourceRange GetCurLine(SourceLocation loc) const;

	// 同GetCurLine，但包括换行符：[该行开头，下一行开头]
	SourceRange GetCurFullLine(SourceLocation loc) const;

	// 根据传入的代码位置返回下一行的范围
	SourceRange GetNextLine(SourceLocation loc) const;

	// 获取行号
	int GetLineNo(SourceLocation loc) const;

	// 获取文件对应的被包含行号
	int GetIncludeLineNo(FileID) const;

	// 获取文件对应的#include含范围
	SourceRange GetIncludeRange(FileID) const;

	// 是否是换行符
	bool IsNewLineWord(SourceLocation loc) const;

	// 获取文件对应的#include所在的行（不含换行符）
	// 注意：假设有一个cpp文件的内容如下，其中第1行和第2行包含的a.h是虽然同一个文件，但FileID是不一样的
	//		1. #include "./a.h"
	//		2. #include "a.h"
	//	现传入第一行a.h对应的FileID，则结果 = #include "./a.h"
	std::string GetBeIncludeLineText(FileID file) const;

	// 记录下依赖名关系（比如文件a引用了文件b中某行的某个类名、函数名）
	inline void UseName(FileID file, FileID beusedFile, const char* name = nullptr, int line = 0);

	// 获取父文件（主文件没有父文件）
	FileID GetParent(FileID child) const;

	// 获取c++中class、struct、union的全名，结果将包含命名空间
	// 例如：传入类C，C属于命名空间A中的命名空间B，则结果将返回：namespace A{ namespace B{ class C; }}
	string GetRecordName(const RecordDecl &recordDecl) const;

	// 新增使用前置声明记录（对于不必要添加的前置声明将在之后进行清理）
	inline void UseForward(SourceLocation loc, const CXXRecordDecl *cxxRecordDecl, const NestedNameSpecifier *specifier = nullptr);

	// 是否允许清理该c++文件（若不允许清理，则文件内容不会有任何变化）
	inline bool CanClean(FileID file) const;
	inline bool CanCleanByName(const char *fileName) const;

	// 获取文件信息
	std::string DebugParentFileText(FileID file, int n) const;

	// 获取该位置所在行的信息：所在行的文本、所在文件名、行号
	std::string DebugLocText(SourceLocation loc) const;

	// 获取文件所使用名称信息：文件名、所使用的类名、函数名、宏名等以及对应行号
	void DebugUsedNames(FileID file, const std::vector<UseNameInfo> &useNames) const;

	// 获取拼写位置
	inline SourceLocation GetSpellingLoc(SourceLocation loc) const;

	// 获取经过宏扩展后的位置
	inline SourceLocation GetExpasionLoc(SourceLocation loc) const;

	// 获取文件ID
	inline FileID GetFileID(SourceLocation loc) const;

	// 获取第1个文件#include第2个文件的文本串
	std::string GetRelativeIncludeStr(FileID f1, FileID f2) const;

	// 根据头文件搜索路径，将绝对路径转换为双引号包围的文本串，
	// 例如：假设有头文件搜索路径"d:/a/b/c" 则"d:/a/b/c/d/e.h" -> "d/e.h"
	string GetQuotedIncludeStr(const char *absoluteFilePath) const;

	// 替换指定范围文本
	void ReplaceText(FileID file, int beg, int end, const char* text);

	// 将文本插入到指定位置之前
	// 例如：假设有"abcdefg"文本，则在c处插入123的结果将是："ab123cdefg"
	void InsertText(FileID file, int loc, const char* text);

	// 移除指定范围文本
	void RemoveText(FileID file, int beg, int end);

	// 移除指定文件内的无用行
	void CleanByDelLine(const FileHistory &history, FileID file);

	// 在指定文件内添加前置声明
	void CleanByForward(const FileHistory &history, FileID file);

	// 在指定文件内替换#include
	void CleanByReplace(const FileHistory &history, FileID file);

	// 在指定文件内新增行
	void CleanByAdd(const FileHistory &history, FileID file);

	// 根据历史清理指定文件
	void CleanByHistory(const FileHistoryMap &historys);

	// 文件格式是否是windows格式，换行符为[\r\n]，类Unix下为[\n]
	bool IsWindowsFormat(FileID) const;

	// 取出对当前cpp文件的分析结果
	void TakeHistorys(FileHistoryMap &out) const;

	// 该文件是否是预编译头文件
	bool IsPrecompileHeader(FileID file) const;

	// 取出本文件的编译错误历史
	void TakeCompileErrorHistory(FileHistoryMap &out) const;

	// a文件是否在b位置之前
	bool IsFileBeforeLoc(FileID a, SourceLocation b) const;

	// a文件是否在b文件之前
	bool IsFileBeforeFile(FileID a, FileID b) const;

	// 将当前cpp文件产生的待清理记录与之前其他cpp文件产生的待清理记录合并
	void MergeTo(FileHistoryMap &old) const;

	// 文件是否已默认被包含
	bool IsDefaultIncluded(FileID file) const;

	// 第2个文件是否是第1个文件的祖先
	inline bool IsAncestorByName(FileID young, FileID old) const;

	// 第2个文件是否是第1个文件的祖先
	inline bool IsAncestorByName(const char *young, const char *old) const;

	// 在最终结果中，文件a是否包含了文件b
	inline bool Contains(FileID a, FileID b) const;

	// 获取该文件第一次被包含时的文件ID（同一文件可能包含多次，对应的有多个文件ID）
	FileID GetFirstFileID(FileID file) const;

	// 获取文件名对应的文件ID（同一文件可能包含多次，对应的有多个文件ID，这里取第一个）
	FileID GetFileIDByFileName(const char *fileName) const;

	// 该文件是否应保留所引用的class、struct、union的前置声明
	bool IsShouldKeepForwardClass(FileID, const CXXRecordDecl &cxxRecord) const;

	// 删掉多余文件，返回值：true本次有删除、false本次未删除
	bool CutInclude(FileID top, FileSet &done, FileSet &includes);

	// 合并包含文件（每个文件记录了应包含的所有的文件，但其中一部分已经被子文件包含过了，可以移除掉）
	bool MergeMinInclude();

	// 是否用户文件（可被修改的文件视为用户文件，其余的视为外部文件）
	inline bool IsUserFile(FileID file) const;

	// 是否外部文件（可被修改的文件视为用户文件，其余的视为外部文件）
	inline bool IsOuterFile(FileID file) const;

	// 获取外部文件祖先
	inline FileID GetOuterFileAncestor(FileID file) const;

	// 生成默认被包含的文件列表
	void GenerateDefaultIncludes();

	// 生成外部文件祖先记录
	void GenerateOutFileAncestor();

	// 生成用户文件的依赖记录
	void GenerateUserUse();

	// 计算出每个文件应包含的最少文件
	void GenerateMinInclude();

	// 生成新增前置声明列表
	void GenerateForwardClass();

	// 裁剪前置声明列表（删掉重复的）
	void MinimizeForwardClass();

	// 获取指定文件及其后代文件的新增前置声明列表
	void GetAllForwardsInKids(FileID top, RecordSet &forwards);

	// 取出单个文件的可删除#include行
	void TakeDel(FileHistory &history, const FileSet &dels) const;

	// 取出头文件替换信息
	void TakeReplaceLine(ReplaceLine &replaceLine, FileID from, FileID to) const;

	// 取出新增前置声明信息
	void TakeForwardClass(FileHistory &history, FileID insertAfter, FileID top) const;

	// 取出新增包含文件信息
	void TakeAdd(FileHistory &history, FileID top, const std::map<FileID, FileVec> &inserts) const;
	
	// 计算下一层的祖先
	FileID GetSecondAncestor(FileID top, FileID child) const;

	// 对新包含的文件进行排序，计算出每个文件应插入的位置
	void SortAddFiles(FileID top, FileSet &adds, FileSet &keeps, FileSet &dels, FileID insertAfter, std::map<FileID, FileVec> &inserts) const;	

	// 计算出应在哪一行旧#include后插入新#include
	FileID CalcInsertLoc(const FileSet &includes, const FileSet &dels) const;

	// 取出对指定文件的分析结果
	void TakeHistory(FileID top, FileHistory &out) const;

	// 祖先文件是否默认被包含
	inline bool IsAncestorDefaultInclude(FileID file) const;

	// 祖先文件是否被强制忽略
	inline bool IsAncestorSkip(FileID file) const;

	// 是否有必要打印该文件
	bool IsNeedPrintFile(FileID) const;

// 打印
private:
	// 打印各文件的父文件
	void PrintParent();

	// 打印引用记录
	void PrintUse() const;

	// 打印#include记录
	void PrintInclude() const;

	// 打印引用类名、函数名、宏名等的记录
	void PrintUseName() const;

	// 打印可转为前置声明的类指针或引用记录
	void PrintUseRecord() const;

	// 打印最终的前置声明记录
	void PrintForwardClass() const;

	// 打印允许被清理的所有文件列表
	void PrintAllFile() const;

	// 打印清理日志
	void PrintHistory() const;

	// 打印各文件内的命名空间
	void PrintNamespace() const;

	// 打印各文件内的using namespace
	void PrintUsingNamespace() const;

	// 打印各文件内的using
	void PrintUsingXXX() const;

	// 打印头文件搜索路径
	void PrintHeaderSearchPath() const;

	// 用于调试：打印各文件重新计算后的#include文本
	void PrintRelativeInclude() const;

	// 打印每个文件的原始后代文件记录
	void PrintKidsByName();

	// 打印被包含多次的文件
	void PrintSameFile() const;

	// 打印每个文件最终应包含的文件和新增的前置声明
	void PrintMinInclude() const;

	// 打印每个文件最终的后代文件
	void PrintMinKid() const;

	// 打印
	void PrintOutFileAncestor() const;

	// 打印
	void PrintUserUse() const;

public:
	// 当前正在解析的文件
	static ParsingFile *g_nowFile;

	//================== 最终分析结果 ==================//
private:
	// [最终分析结果]. 分析当前cpp文件的结果：[c++文件名] -> [该文件的清理结果]
	FileHistoryMap								m_historys;

	// 分析结果：各文件的最小包含文件列表：[文件ID] -> [该文件仅应直接包含的文件ID列表]
	std::map<FileID, FileSet>					m_minInclude;

	// 分析结果：每个文件最终应新增的前置声明
	FileUseRecordsMap							m_fowardClass;


	//================== [对原始数据进行分析后的结果] ==================//
private:
	// 各文件的后代文件名列表：[文件名] -> [该文件包含的全部后代文件]
	std::map<std::string, FileNameSet>			m_kidsByName;

	// 各文件应包含的后代文件列表：[文件ID] -> [该文件应包含的后代文件ID列表]
	std::map<FileID, FileSet>					m_minKids;

	// 用户文件列表（可被修改的文件视为用户文件，其余的视为外部文件，比如，假设某文件中有#include <vector>，因为<vector>是库文件，禁止被改动，所以vector是外部文件）
	FileSet										m_userFiles;

	// 各外部文件的祖先外部文件：[文件ID] -> [对应的祖先外部文件ID]
	std::map<FileID, FileID>					m_outFileAncestor;

	// 项目内文件的引用关系：[用户文件名] -> [所引用的用户文件ID列表 + 外部文件ID列表]
	std::map<std::string, FileSet>				m_userUses;

	// 默认被包含的文件ID列表（这些文件的所有后代文件都不允许被修改）
	FileSet										m_defaultIncludes;

	// 被强制忽略的文件ID列表（这些文件的所有后代文件都不允许被修改）
	FileSet										m_skips;


	//================== [原始数据] ==================//
private:
	//------ 1. 依赖关系 ------//

	// 各文件引用其他文件的记录：[文件ID] -> [引用的其他文件列表]（例如，假设A.h用到了B.h中的class B，则认为A.h引用了B.h）
	std::map<FileID, FileSet>					m_uses;

	// 仅用于打印：各文件所使用的类名、函数名、宏名等的名称记录：[文件ID] -> [该文件所使用的其他文件中的类名、函数名、宏名、变量名等]
	std::map<FileID, std::vector<UseNameInfo>>	m_useNames;

	//------ 2. 使用类、结构体的记录 ------//

	// 每个位置所使用的class、struct（指针、引用），用于生成前置声明：[位置] -> [所使用的class、struct、union指针或引用]
	LocUseRecordsMap							m_locUseRecordPointers;

	// 每个文件所使用的class、struct（指针、引用），用于生成前置声明：[文件] -> [所使用的class、struct、union指针或引用]
	FileUseRecordsMap							m_fileUseRecordPointers;

	// 每个文件所使用的class、struct（非指针、非引用），用于避免生成多余的前置声明
	FileUseRecordsMap							m_fileUseRecords;

	//------ 3. 与using有关的记录 ------//

	// using namespace记录（例如：using namespace std;）：[using namespace的位置] -> [对应的namespace定义]
	typedef map<SourceLocation, const NamespaceDecl*> UsingNamespaceLocMap;
	map<SourceLocation, const NamespaceDecl*>	m_usingNamespaces;

	// 各文件中的using namespace记录
	map<FileID, UsingNamespaceLocMap>			m_usingNamespacesByFile;
	
	// using记录（例如：using std::string;）：[using的目标对应的位置] -> [using声明]
	UsingVec									m_usings;

	// 各文件中的using记录
	typedef std::map<FileID, UsingVec> UsingByFileMap;
	UsingByFileMap								m_usingsByFile;

	// 仅用于打印：各文件内声明的命名空间记录：[文件] -> [该文件内的命名空间记录]
	std::map<FileID, std::set<std::string>>		m_namespaces;

	//------ 4. 文件、文件名 ------//

	// 各文件所包含的文件集合：[文件名] -> [所include的文件集合]
	std::map<std::string, FileSet>				m_includes;

	// 所有文件ID
	FileSet										m_files;

	// 父文件关系：[文件ID] -> [父文件ID]
	std::map<FileID, FileID>					m_parents;

	// 同一个文件名对应的不同文件ID：[文件名] -> [同名文件ID列表]
	std::map<std::string, FileSet>				m_sameFiles;

	// 所有文件ID对应的文件名：[文件ID] -> [文件名]
	std::map<FileID, std::string>				m_fileNames;

	// 所有文件ID对应的文件名：[文件ID] -> [小写文件名]
	std::map<FileID, std::string>				m_lowerFileNames;

	// 文件名对应的文件ID：[文件名] -> [文件ID]
	std::map<std::string, FileID>				m_fileNameToFileIDs;	

	// 头文件搜索路径列表
	std::vector<HeaderSearchDir>				m_headerSearchPaths;
	
	// 主文件id
	FileID										m_root;

	//================== [clang数据] ==================//
private:
	// clang文件重写类，用来修改c++源码内容
	clang::Rewriter								m_rewriter;

	// clang源码管理器
	clang::SourceManager*						m_srcMgr;

	// clang编译实例
	clang::CompilerInstance*					m_compiler;

	// 本文件的编译错误历史
	CompileErrorHistory							m_compileErrorHistory;

	// 当前打印索引，仅用于日志打印
	mutable int									m_printIdx;
};
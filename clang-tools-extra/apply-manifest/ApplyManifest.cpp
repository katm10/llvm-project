// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Option/ArgList.h"
#include <sstream>
#include <fstream>

using namespace clang::tooling;
using namespace clang;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory ApplyManifestCategory("apply-manifest options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static llvm::cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static llvm::cl::extrahelp MoreHelp("\nFill this in later...\n");


typedef std::map<std::string, std::vector<int>> manifest_t;


std::string file_to_work;

class TraceInserter: public RecursiveASTVisitor<TraceInserter> {
public:
	manifest_t *manifest;
	TraceInserter(Rewriter &R, manifest_t *m): ConditionRewriter(R), manifest(m) {}
	
	bool VisitStmt(Stmt *s) {
		if (isa<IfStmt>(s)) {
			IfStmt *IfStatement = cast<IfStmt>(s);
			SourceLocation startLoc = IfStatement->getLParenLoc().getLocWithOffset(1);
			SourceLocation endLoc = IfStatement->getRParenLoc();


		

			std::stringstream SSBefore;
			if (manifest && (*manifest)[current_function].size() > unique_counter) {
				if ((*manifest)[current_function][unique_counter] == 0) {
					SSBefore << "mpns_unlikely(";
				} else if ((*manifest)[current_function][unique_counter] == 1){
					SSBefore << "mpns_likely(";
				} else {
					SSBefore << "mpns_unknown(";
				}
			} else
				SSBefore << "mpns_unknown(";
			SSBefore << "(";
			ConditionRewriter.InsertText(startLoc, SSBefore.str(), true, true);
			
			std::stringstream SSAfter;
			SSAfter << ") && 1, \"" << current_function << "\", " << unique_counter << ", &" << current_function <<  ")";
			unique_counter++;	
			ConditionRewriter.InsertText(endLoc, SSAfter.str(), true, true);
	
					
		} else if (isa<SwitchStmt>(s)) {
			unique_counter++;
			llvm::errs() << "Applying manifest to Switch Statements not supported\n";	
		}
		return true;
	}
	bool inManifest(std::string fname) {
		if (!manifest)
			return true;
		if (manifest->find(fname) != manifest->end())
			return true;
		else
			return false;
	}
	bool VisitFunctionDecl(FunctionDecl *f) {
		// Only function definitions (with bodies), not declarations.
		current_function = "";
		bool to_add = true;
		if (f->hasBody()) {
			SourceManager &SM = ConditionRewriter.getSourceMgr();
			SourceLocation SL = f->getLocation();
			PresumedLoc PLoc = SM.getPresumedLoc(SL);
			if (PLoc.getFilename() != file_to_work)
				return false;
			Stmt *FuncBody = f->getBody();

			// Type name as string
			QualType QT = f->getReturnType();
			std::string TypeStr = QT.getAsString();

			// Function name
			DeclarationName DeclName = f->getNameInfo().getName();
			std::string FuncName = DeclName.getAsString();
			current_function = FuncName;
		
			to_add = !inManifest(current_function);

			// Add comment before
			std::stringstream SSBefore;
			SSBefore << "// Begin function " << FuncName << " returning " << TypeStr
				<< "\n";
			if (to_add) 
				SSBefore << "#if 0\n";
			SourceLocation ST = f->getSourceRange().getBegin();
			ConditionRewriter.InsertText(ST, SSBefore.str(), true, true);

			// And after
			std::stringstream SSAfter;
			if (to_add)
				SSAfter << "\n#endif";
			SSAfter << "\n// End function " << FuncName;
			ST = FuncBody->getEndLoc().getLocWithOffset(1);
			ConditionRewriter.InsertText(ST, SSAfter.str(), true, true);
		}
		return !to_add;
	}

	int unique_counter;
private:
	Rewriter &ConditionRewriter;
	std::string current_function;
	bool decl_inserted = false;
};

class ApplyManifestASTConsumer: public ASTConsumer {
public: 
	ApplyManifestASTConsumer(Rewriter &R, manifest_t *m): Visitor(R, m), manifest(m) {}
	virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
		for(DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
			Visitor.unique_counter = 0;		
			Visitor.TraverseDecl(*b);
		}	
		return true;
	}

private:
	TraceInserter Visitor;
	manifest_t *manifest;

};


class ApplyManifestFrontendAction : public ASTFrontendAction {
public:
	static manifest_t *manifest;
	ApplyManifestFrontendAction()  {}
	void EndSourceFileAction() override {
		SourceManager &SM = TheRewriter.getSourceMgr();

		const RewriteBuffer *RewriteBuf =
			TheRewriter.getRewriteBufferFor(SM.getMainFileID());
                if(RewriteBuf == nullptr)
			return;
/*
		llvm::outs() << "extern int __trace_condition(const char*, int, int);\n";
		llvm::outs() << "extern int __trace_function(const char*);\n";
		llvm::outs() << "extern int __trace_switch(const char*, int, int);\n";
*/
		llvm::outs() << "extern void mpns_abort(char*, int, void*);\n";
		llvm::outs() << "static inline int mpns_likely(int condition, char* name, int id, void* fn) {if (!condition) mpns_abort(name, id, fn); return 1;}\n";
		llvm::outs() << "static inline int mpns_unlikely(int condition, char* name, int id, void* fn) {if (condition) mpns_abort(name, id, fn); return 0;}\n";
		llvm::outs() << "static inline int mpns_unknown(int condition, char* name, int id, void* fn) {return condition;}\n";

		llvm::outs() << "extern void* __translate_function(void*);\n";
		llvm::outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
			StringRef file) override {
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		return std::make_unique<ApplyManifestASTConsumer>(TheRewriter, manifest);
	}

private:
	Rewriter TheRewriter;
};
manifest_t *ApplyManifestFrontendAction::manifest;

manifest_t read_manifest(const char* filename, int *no_manifest) {
	manifest_t to_return;
	*no_manifest = 0;
	std::fstream f;
	f.open(filename, std::fstream::in);
	
	int total_f = 0;
	f >> total_f;
	if (total_f < 0) {
		*no_manifest = 1;
		return to_return;
	}
		
	for (int i = 0; i < total_f; i++) {
		std::string fname; int fcount; int flimit;
		f >> fname >> flimit >> fcount;
		to_return[fname] = std::vector<int>(flimit);
		for (int j = 0; j < flimit; j++) {
			to_return[fname][j] = -1;
		}
		for (int j = 0; j < fcount; j++) {
			int offset, value;
			f >> offset >> value;
			to_return[fname][offset] = value;
		}
	}
	return to_return;
}

int main(int argc, const char **argv) {

	file_to_work = argv[1];
	argc--;
	argv++;
	// Extract the manifest file name and forward rest of the args
	const char *manifest_fname = argv[1];
	argc--;
	argv++;	
	int no_manifest;	
	manifest_t manifest = read_manifest(manifest_fname, &no_manifest);
	
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, ApplyManifestCategory);
	if (!ExpectedParser) {
    // Fail gracefully for unsupported options.
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser& OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  
	// ClangTool::run accepts a FrontendActionFactory, which is then used to
	// create new objects implementing the FrontendAction interface. Here we use
	// the helper newFrontendActionFactory to create a default factory that will
	// return a new MyFrontendAction object every time.
	// To further customize this, we could create our own factory class.
	if (!no_manifest) {
		ApplyManifestFrontendAction::manifest = &manifest;
	} else {
		ApplyManifestFrontendAction::manifest = nullptr;
	}
	return Tool.run(newFrontendActionFactory<ApplyManifestFrontendAction>().get());
}

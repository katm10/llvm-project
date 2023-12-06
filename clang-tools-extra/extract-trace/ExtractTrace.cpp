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

using namespace clang::tooling;
using namespace clang;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory ExtractTraceCategory("extract-trace options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static llvm::cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static llvm::cl::extrahelp MoreHelp("\nFill this in later...\n");


std::string file_to_work;

class TraceInserter: public RecursiveASTVisitor<TraceInserter> {
public:
	TraceInserter(Rewriter &R): ConditionRewriter(R) {}
	
	bool VisitStmt(Stmt *s) {
		if (isa<IfStmt>(s)) {
			IfStmt *IfStatement = cast<IfStmt>(s);
			SourceLocation startLoc = IfStatement->getLParenLoc().getLocWithOffset(1);
			SourceLocation endLoc = IfStatement->getRParenLoc();

			std::stringstream SSBefore;
			SSBefore << "__trace_condition(\"";
			SSBefore << current_function;
			SSBefore << "\", " << unique_counter << ", (";
			unique_counter++;	
			ConditionRewriter.InsertText(startLoc, SSBefore.str(), true, true);
			
			std::stringstream SSAfter;
			SSAfter << ") && 1)";
			ConditionRewriter.InsertText(endLoc, SSAfter.str(), true, true);
	
					
		} else if (isa<SwitchStmt>(s)) {
			SwitchStmt *SwitchStatement = cast<SwitchStmt>(s);
			SourceLocation startLoc = SwitchStatement->getLParenLoc().getLocWithOffset(1);
			SourceLocation endLoc = SwitchStatement->getRParenLoc();
	
			std::stringstream SSBefore;
			SSBefore << "__trace_switch(\"";
			SSBefore << current_function;
			SSBefore << "\", " << unique_counter << ", (";
			unique_counter++;
			ConditionRewriter.InsertText(startLoc, SSBefore.str(), true, true);
		
			std::stringstream SSAfter;
			SSAfter << "))";
			
			ConditionRewriter.InsertText(endLoc, SSAfter.str(), true, true);
				
		}
		return true;
	}
	bool VisitFunctionDecl(FunctionDecl *f) {
		// Only function definitions (with bodies), not declarations.
			
		if (f->hasBody()) {
			SourceManager &SM = ConditionRewriter.getSourceMgr();
			SourceLocation SL = f->getLocation();
			PresumedLoc PLoc = SM.getPresumedLoc(SL);
			//if (PLoc.getFilename() != file_to_work)
			//	return false;


			DeclarationName DeclName = f->getNameInfo().getName();
			std::string FuncName = DeclName.getAsString();
			//llvm::errs() << "//" << FuncName << " is in " << SL.printToString(SM) << "\n";
			Stmt *FuncBody = f->getBody();

			// Type name as string
			QualType QT = f->getReturnType();
			std::string TypeStr = QT.getAsString();

			// Function name
			current_function = FuncName;

			if (CompoundStmt::classof(FuncBody)) {
				CompoundStmt* FuncBodyBlock = static_cast<CompoundStmt*>(FuncBody);
				std::stringstream SSStart;
				SSStart << "__trace_function(\"";
				SSStart << current_function;
				SSStart << "\");";
				SourceLocation ST = FuncBodyBlock->getLBracLoc().getLocWithOffset(1);
				ConditionRewriter.InsertText(ST, SSStart.str(), true, true);
			}

			// Add comment before
			std::stringstream SSBefore;
			SSBefore << "// Begin function " << FuncName << " returning " << TypeStr
				<< "\n";
			SourceLocation ST = f->getSourceRange().getBegin();
			ConditionRewriter.InsertText(ST, SSBefore.str(), true, true);

			// And after
			std::stringstream SSAfter;
			SSAfter << "\n// End function " << FuncName;
			ST = FuncBody->getEndLoc().getLocWithOffset(1);
			ConditionRewriter.InsertText(ST, SSAfter.str(), true, true);
		}
		return true;
	}

	int unique_counter;
private:
	Rewriter &ConditionRewriter;
	std::string current_function;
	bool decl_inserted = false;
};

class ExtractTraceASTConsumer: public ASTConsumer {
public: 
	ExtractTraceASTConsumer(Rewriter &R): Visitor(R) {}
	virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
		for(DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
			Visitor.unique_counter = 0;		
			Visitor.TraverseDecl(*b);
		}	
		return true;
	}

private:
	TraceInserter Visitor;

};


class ExtractTraceFrontendAction : public ASTFrontendAction {
public:
	ExtractTraceFrontendAction() {}
	void EndSourceFileAction() override {
		SourceManager &SM = TheRewriter.getSourceMgr();

		const RewriteBuffer *RewriteBuf =
			TheRewriter.getRewriteBufferFor(SM.getMainFileID());
                if(RewriteBuf == nullptr)
			return;
		llvm::outs() << "extern int __trace_condition(const char*, int, int);\n";
		llvm::outs() << "extern int __trace_function(const char*);\n";
		llvm::outs() << "extern int __trace_switch(const char*, int, int);\n";

		llvm::outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
			StringRef file) override {
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		return std::make_unique<ExtractTraceASTConsumer>(TheRewriter);
	}

private:
	Rewriter TheRewriter;
};

int main(int argc, const char **argv) { 
	file_to_work = argv[1];
	argv++;
	argc--;
	auto ExpectedParser = CommonOptionsParser::create(argc, argv, ExtractTraceCategory);
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
	return Tool.run(newFrontendActionFactory<ExtractTraceFrontendAction>().get());
}

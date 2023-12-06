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

std::string file_to_work;



class TraceInserter: public RecursiveASTVisitor<TraceInserter> {
public:
	TraceInserter(Rewriter &R): ConditionRewriter(R) {}

	bool VisitUnaryOperator(UnaryOperator *op) {	
		return true;
	}
/*
	bool TraverseUnaryOperator(UnaryOperator *op) {
		if (op->getOpcode() == UO_AddrOf) {
			return true;
		}	
		return RecursiveASTVisitor<TraceInserter>::TraverseUnaryOperator(op);
	}
*/
	
		

	bool TraverseVarDecl(VarDecl *decl) {
		if (decl->hasGlobalStorage()) {
			llvm::outs() << "void* " << decl->getNameAsString() << "__addr(void);" << "\n";
			// We will also not visit the RHS of the decl because it could have reference to other globals
			return true;
		}		
		return RecursiveASTVisitor<TraceInserter>::TraverseVarDecl(decl);
	}
	bool VisitDeclRefExpr(DeclRefExpr * expr) {
		SourceManager &SM = ConditionRewriter.getSourceMgr();
		SourceLocation SL = expr->getBeginLoc();
		PresumedLoc PLoc = SM.getPresumedLoc(SL);
		if (PLoc.getFilename() != file_to_work)
			return false;
		ValueDecl* decl = expr->getDecl();
		if (isa<VarDecl>(decl)) {
			VarDecl *vdecl = cast<VarDecl>(decl);
			if (vdecl->hasGlobalStorage()) {
				llvm::errs() << "Accessed global " << vdecl->getNameAsString() << "\n";
				SourceLocation start = expr->getBeginLoc();

				std::stringstream SSBefore;

				QualType ptr_type = vdecl->getASTContext().getPointerType(vdecl->getType());
				std::string ptr_type_str = ptr_type.getAsString();
				

				SSBefore << "(*(" << ptr_type_str << ")(";
				ConditionRewriter.InsertText(start, SSBefore.str(), true, true);
				
				SourceLocation end = expr->getEndLoc().getLocWithOffset(vdecl->getNameAsString().size());
				std::stringstream SSAfter;
				SSAfter << "__addr()))";
				ConditionRewriter.InsertText(end, SSAfter.str(), true, true);
					
			}
			
		}
		return true;
	}
	bool VisitFunctionDecl(FunctionDecl *f) {
		SourceManager &SM = ConditionRewriter.getSourceMgr();
		SourceLocation SL = f->getLocation();
		PresumedLoc PLoc = SM.getPresumedLoc(SL);
		if (PLoc.getFilename() != file_to_work)
			return false;
		// Only function definitions (with bodies), not declarations.
		current_function = "";
		if (f->hasBody()) {
			Stmt *FuncBody = f->getBody();

			// Type name as string
			QualType QT = f->getReturnType();
			std::string TypeStr = QT.getAsString();

			// Function name
			DeclarationName DeclName = f->getNameInfo().getName();
			std::string FuncName = DeclName.getAsString();
			current_function = FuncName;

			std::stringstream SSAfter;
			SSAfter << "\n//\n";
			auto ST = FuncBody->getEndLoc().getLocWithOffset(1);
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

class ApplyManifestASTConsumer: public ASTConsumer {
public: 
	ApplyManifestASTConsumer(Rewriter &R): Visitor(R){}
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


class ApplyManifestFrontendAction : public ASTFrontendAction {
public:
	ApplyManifestFrontendAction()  {}
	void EndSourceFileAction() override {
		SourceManager &SM = TheRewriter.getSourceMgr();

		const RewriteBuffer *RewriteBuf =
			TheRewriter.getRewriteBufferFor(SM.getMainFileID());
                if(RewriteBuf == nullptr)
			return;
		
		llvm::outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
			StringRef file) override {
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		return std::make_unique<ApplyManifestASTConsumer>(TheRewriter);
	}

private:
	Rewriter TheRewriter;
};


int main(int argc, const char **argv) {
	file_to_work = argv[1];
	argc--;
	argv++;

	// Extract the manifest file name and forward rest of the args
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
	return Tool.run(newFrontendActionFactory<ApplyManifestFrontendAction>().get());
}

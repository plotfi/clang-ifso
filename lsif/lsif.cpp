#include "clang/AST/Decl.h"
#include "clang/AST/Mangle.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Index/CodegenNameGenerator.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Option/OptTable.h"
#include "clang/Driver/Options.h"

using namespace clang;
using namespace clang::index;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

using namespace driver;
using namespace options;
using namespace llvm::opt;

namespace clang {
namespace ast_matchers {
namespace {

AST_MATCHER(NamedDecl, isVisible) {
  return Node.getVisibility() == DefaultVisibility;
}

AST_POLYMORPHIC_MATCHER(hasDiscardableLinkage,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(FunctionDecl,
                                                        VarDecl)) {
  return false;
}

AST_MATCHER_P(ObjCImplementationDecl, hasInterface,
              ::clang::ast_matchers::internal::Matcher<ObjCInterfaceDecl>,
              InnerMatcher) {
  return true;
}

} // namespace
} // namespace ast_matchers
} // namespace clang

bool writeFuncOrVarName(MangleContext *MC, const NamedDecl *D,
    raw_ostream &OS) {
  if (MC->shouldMangleDeclName(D)) {
    if (const auto *CtorD = dyn_cast<CXXConstructorDecl>(D))
      MC->mangleCXXCtor(CtorD, Ctor_Complete, OS);
    else if (const auto *DtorD = dyn_cast<CXXDestructorDecl>(D))
      MC->mangleCXXDtor(DtorD, Dtor_Complete, OS);
    else
      MC->mangleName(D, OS);
    return false;
  } else if (IdentifierInfo *II = D->getIdentifier()) {
    OS << II->getName();
    return false;
  }

  assert(false && "Invalid NamedDecl: can't mangle name.");
  return true;
}

class YamlPrintManager {
  bool IsHeaderPrinted = false;
  std::vector<std::string> Names;
  public:
  ~YamlPrintManager() {
    llvm::errs() << "  Global:\n";
    for (auto Name : Names) {
      llvm::errs() << "    - Name:            " << Name << "\n";
      llvm::errs() << "      Type:            STT_FUNC\n";
      llvm::errs() << "      Section:         .text\n";
    }

    llvm::errs() << "DynamicSymbols:\n";

    llvm::errs() << "  Global:\n";
    for (auto Name : Names) {
      llvm::errs() << "    - Name:            " << Name << "\n";
      llvm::errs() << "      Type:            STT_FUNC\n";
      llvm::errs() << "      Section:         .text\n";
    }

    llvm::errs() << "...\n";
  }

#if 0
  void PrintHeader(const CompilerInstance &CI) {
    if (IsHeaderPrinted)
      return;

    llvm::errs() << "--- !ELF\n";
    llvm::errs() << "FileHeader:\n";
    llvm::errs() << "  Class:           ELFCLASS";
    llvm::errs() << (CI.getTarget().getTriple().isArch64Bit() ? "64" : "32");
    llvm::errs() << "\n";
    llvm::errs() << "  Data:            ELFDATA2";
    llvm::errs() << (CI.getTarget().getTriple().isLittleEndian() ? "LSB" : "MSB");
    llvm::errs() << "\n";
    llvm::errs() << "  Type:            ET_DYN\n";
    llvm::errs() << "  Machine:         ";
    StringRef ArchName = CI.getTarget().getTriple().getArchName();
    // llvm::errs() << "  ArchName:         " << ArchName << "\n";

    StringRef MachineType = llvm::StringSwitch<StringRef>(ArchName)
      .Case("x86_64", "EM_X86_64")
      .Case("x86", "EM_386")
      .Case("i386", "EM_386")
      .Case("i686", "EM_386")
      .Case("aarch64", "EM_AARCH64")
      .Case("arm", "EM_ARM")
      .Default("EM_X86_64");

    llvm::errs() << MachineType;
    llvm::errs() << "\n";

    // Str = printEnum(e->e_machine, makeArrayRef(ElfMachineType));
    // printFields(OS, "Machine:", Str);

    llvm::errs() << "Sections:\n";
    llvm::errs() << "  - Name:            .text\n";
    llvm::errs() << "    Type:            SHT_PROGBITS\n";
    llvm::errs() << "Symbols:\n";
    llvm::errs() << "  Local:\n";
    llvm::errs() << "    - Name:            .dynsym\n";
    llvm::errs() << "      Type:            STT_SECTION\n";
    llvm::errs() << "      Section:         .dynsym\n";
    llvm::errs() << "    - Name:            .dynstr\n";
    llvm::errs() << "      Type:            STT_SECTION\n";
    llvm::errs() << "      Section:         .dynstr\n";

    IsHeaderPrinted = true;
  }
#endif

  void addName(std::string Name) {
    Names.push_back(Name);
  }
};

YamlPrintManager YPM;

class FindVisible : public MatchFinder::MatchCallback {
public:
  void run(const MatchFinder::MatchResult &Result) override {
    const auto ND = Result.Nodes.getNodeAs<NamedDecl>("Definition");
    assert(ND && "expected matching NamedDecl named 'Definition'");

    std::string Name;
    SmallString<128> FrontendBuf;
    llvm::raw_svector_ostream FrontendBufOS(FrontendBuf);
    MangleContext *MC = ND->getASTContext().createMangleContext();
    if (auto *FTD = dyn_cast<FunctionTemplateDecl>(ND)) {
      if (!writeFuncOrVarName(MC, FTD->getTemplatedDecl(), FrontendBufOS))
        Name = FrontendBufOS.str();
    } else {
      if (!writeFuncOrVarName(MC, ND, FrontendBufOS))
        Name = FrontendBufOS.str();
    }

    errs() << Name << "\n";
  }

  bool ProducedSymbols = false;
};

int main(int argc, const char **argv) {
  cl::OptionCategory Category("lsif options");
  CommonOptionsParser Opt(argc, argv, Category);


  std::unique_ptr<opt::OptTable> Opts = createDriverOptTable();
  const unsigned IncludedFlagsBitmask = options::CC1Option;
  InputArgList Args =
    Opts->ParseArgs(llvm::makeArrayRef(argv + 1, argc), MissingArgIndex);

 // std::string CPU = Args->getLastArgValue(OPT_target_cpu);
 //  std::string Triple = Args->getLastArgValue(OPT_triple);

  ClangTool Tool(Opt.getCompilations(), Opt.getSourcePathList());

  // Silence superfluous "'linker' input unused" warnings.
  auto Adjuster = getInsertArgumentAdjuster("-Qunused-arguments");
  Tool.appendArgumentsAdjuster(Adjuster);

  auto VisibleDef =
      allOf(isDefinition(), isVisible(), unless(hasDiscardableLinkage()),
            unless(isStaticStorageClass()));
  auto Matcher = namedDecl(anyOf(
      functionDecl(VisibleDef), varDecl(VisibleDef, unless(hasLocalStorage())),
      objcImplementationDecl(hasInterface(isVisible()))));

  FindVisible Handler;
  MatchFinder Finder;
  Finder.addMatcher(id("Definition", Matcher), &Handler);

  auto Success = 0 == Tool.run(newFrontendActionFactory(&Finder).get());
  return Success && Handler.ProducedSymbols ? EXIT_SUCCESS : EXIT_FAILURE;
}


//===- PrintFunctionNames.cpp ---------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Example clang plugin which simply prints the names of all the top-level decls
// in the input file.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
using namespace clang;

namespace {

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
  } else {
    IdentifierInfo *II = D->getIdentifier();
    if (!II)
      return true;
    OS << II->getName();
    return false;
  }
}

#define DEBUG_IFSO 0

class PrintFunctionsConsumer : public ASTConsumer {
  CompilerInstance &Instance;
  std::set<std::string> ParsedTemplates;

public:
  PrintFunctionsConsumer(CompilerInstance &Instance,
                         std::set<std::string> ParsedTemplates)
      : Instance(Instance), ParsedTemplates(ParsedTemplates) {}

  std::vector<std::string> Names;

  virtual ~PrintFunctionsConsumer() {
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

  bool HandleTopLevelDecl(DeclGroupRef DG) override {

    for (DeclGroupRef::iterator i = DG.begin(), e = DG.end(); i != e; ++i) {
      const Decl *D = *i;
      SmallString<128> FrontendBuf;
      llvm::raw_svector_ostream FrontendBufOS(FrontendBuf);

      if (const NamedDecl *ND = dyn_cast<NamedDecl>(D)) {

        #if DEBUG_IFSO
        llvm::errs() << "[NamedDecl] top-level-decl: " << ND->getName();
        #endif
        bool isWeak = false;
        if (const ValueDecl *VD = dyn_cast<ValueDecl>(D)) {
          isWeak = VD->isWeak();
          #if DEBUG_IFSO
          llvm::errs() << " ValueDecl ";
          #endif
        }

        #if DEBUG_IFSO
        llvm::errs() << " isWeak: " << (isWeak ? "yes" : "no") << "\n";
        #endif

        if (D->isFunctionOrFunctionTemplate()) {
          if (D->isTemplateDecl()) {
            MangleContext *MC = ND->getASTContext().createMangleContext();
            std::string Name = writeFuncOrVarName(MC, ND, FrontendBufOS)
                                   ? D->getDescribedTemplate()->getName()
                                   : FrontendBufOS.str();
            Names.push_back(Name);
            #if DEBUG_IFSO
            llvm::errs() << "\t[TemplateDecl] top-level-decl: " << Name << "\n";
            #endif
          } else {
            MangleContext *MC = ND->getASTContext().createMangleContext();
            std::string Name = writeFuncOrVarName(MC, ND, FrontendBufOS)
                                   ? D->getAsFunction()->getName()
                                   : FrontendBufOS.str();
            Names.push_back(Name);
            #if DEBUG_IFSO
            llvm::errs() << "\t[FunctionDecl] top-level-decl: " << Name << "\n";
            #endif

            if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
              if (FunctionTemplateDecl *FTD =
                      FD->getDescribedFunctionTemplate()) {

                bool isWeak = false;
                if (const ValueDecl *VD = dyn_cast<ValueDecl>(FTD)) {
                  isWeak = VD->isWeak();
                  #if DEBUG_IFSO
                  llvm::errs() << "<<< FTD ~~~~~~~ValueDecl ";
                  #endif
                }
                #if DEBUG_IFSO
                llvm::errs() << " isWeak: " << (isWeak ? "yes" : "no") << "\n";
                #endif
              }
            }
          }
        } else {
          if (const NamespaceDecl *NSD = dyn_cast<NamespaceDecl>(D)) {

            #if DEBUG_IFSO
            llvm::errs() << "\t[NamespaceDecl] top-level-decl: \""
                         << NSD->getName() << "\"\n";
            #endif

            for (auto *I : NSD->decls())
              if (const NamedDecl *ND = dyn_cast<NamedDecl>(I)) {

                #if DEBUG_IFSO
                llvm::errs()
                    << "\t\t[NamespaceDecl Decl] namespace-level-decl: \""
                    << ND->getName() << "\"";
                #endif

                #if 0
                ND->dump();
                if (ND->isFunctionOrFunctionTemplate()) {
                        MangleContext *MC = ND->getASTContext().createMangleContext();
                        std::string Name = writeFuncOrVarName(MC, ND, FrontendBufOS)
                          ? ND->getName()
                          : FrontendBufOS.str();
                        Names.push_back(Name);
                }
                #endif

                if (ND->isTemplateDecl()) {

                  #if DEBUG_IFSO
                  llvm::errs() << " ~~~~~~~ isTemplated!! ";
                  #endif

                  auto TD = dyn_cast<TemplateDecl>(I);
                  auto RealTD = TD->getTemplatedDecl();

                  if (const ValueDecl *VD = dyn_cast<ValueDecl>(RealTD)) {

                    #if DEBUG_IFSO
                    llvm::errs() << " Templated ISWEAK!!! ";
                    #endif
                  }
                }

                bool isWeak = false;
                if (const ValueDecl *VD = dyn_cast<ValueDecl>(I)) {
                  isWeak = VD->isWeak();
                  #if DEBUG_IFSO
                  llvm::errs() << " ~~~~~~~ValueDecl ";
                  #endif
                }
                #if DEBUG_IFSO
                llvm::errs() << " isWeak: " << (isWeak ? "yes" : "no") << "\n";
                #endif
              }
          }
        }
      }
    }

    return true;
  }

  void HandleTranslationUnit(ASTContext &context) override {
    if (!Instance.getLangOpts().DelayedTemplateParsing)
      return;

    // This demonstrates how to force instantiation of some templates in
    // -fdelayed-template-parsing mode. (Note: Doing this unconditionally for
    // all templates is similar to not using -fdelayed-template-parsig in the
    // first place.)
    // The advantage of doing this in HandleTranslationUnit() is that all
    // codegen (when using -add-plugin) is completely finished and this can't
    // affect the compiler output.
    struct Visitor : public RecursiveASTVisitor<Visitor> {
      const std::set<std::string> &ParsedTemplates;
      Visitor(const std::set<std::string> &ParsedTemplates)
          : ParsedTemplates(ParsedTemplates) {}
      bool VisitFunctionDecl(FunctionDecl *FD) {
        if (FD->isLateTemplateParsed() &&
            ParsedTemplates.count(FD->getNameAsString()))
          LateParsedDecls.insert(FD);
        return true;
      }

      std::set<FunctionDecl *> LateParsedDecls;
    } v(ParsedTemplates);
    v.TraverseDecl(context.getTranslationUnitDecl());
    clang::Sema &sema = Instance.getSema();
    for (const FunctionDecl *FD : v.LateParsedDecls) {
      clang::LateParsedTemplate &LPT =
          *sema.LateParsedTemplateMap.find(FD)->second;
      sema.LateTemplateParser(sema.OpaqueParser, LPT);
      llvm::errs() << "late-parsed-decl: \"" << FD->getNameAsString() << "\"\n";
    }
  }
};

class PrintFunctionNamesAction : public PluginASTAction {
  std::set<std::string> ParsedTemplates;

protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    llvm::errs() << "--- !ELF\n";
    llvm::errs() << "FileHeader:\n";
    llvm::errs() << "  Class:           ELFCLASS64\n";
    llvm::errs() << "  Data:            ELFDATA2LSB\n";
    llvm::errs() << "  Type:            ET_DYN\n";
    llvm::errs() << "  Machine:         EM_X86_64\n";
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

    return llvm::make_unique<PrintFunctionsConsumer>(CI, ParsedTemplates);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    for (unsigned i = 0, e = args.size(); i != e; ++i) {
      llvm::errs() << "PrintFunctionNames arg = " << args[i] << "\n";

      // Example error handling.
      DiagnosticsEngine &D = CI.getDiagnostics();
      if (args[i] == "-an-error") {
        unsigned DiagID = D.getCustomDiagID(DiagnosticsEngine::Error,
                                            "invalid argument '%0'");
        D.Report(DiagID) << args[i];
        return false;
      } else if (args[i] == "-parse-template") {
        if (i + 1 >= e) {
          D.Report(D.getCustomDiagID(DiagnosticsEngine::Error,
                                     "missing -parse-template argument"));
          return false;
        }
        ++i;
        ParsedTemplates.insert(args[i]);
      }
    }
    if (!args.empty() && args[0] == "help")
      PrintHelp(llvm::errs());

    return true;
  }
  void PrintHelp(llvm::raw_ostream &ros) {
    ros << "Help for PrintFunctionNames plugin goes here\n";
  }
};

} // namespace

static FrontendPluginRegistry::Add<PrintFunctionNamesAction>
    X("print-fns", "print function names");

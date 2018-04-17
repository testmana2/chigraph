/// \file CCompiler.hpp

#ifndef CHI_C_COMPILER_HPP
#define CHI_C_COMPILER_HPP

#include "chi/Fwd.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/utility/string_view.hpp>

namespace chi {

/// Use clang to compile C source code to a llvm module
/// It also uses `stdCIncludePaths` to find basic include paths.
/// \param[in] clangPath The path to the `clang` executable.
/// \param[in] llvmContext The LLVM context to create the module into
/// \param[in] arguments The arguments to clang. Can include input files if desired.
/// \param[in] inputCCode The C code to compile. If `arguments` contains input files, then this can
/// be empty.
/// \param[out] toFill The unique pointer module to create the module inside
/// \return The Result
Result compileCToLLVM(const boost::filesystem::path& clangPath, llvm::LLVMContext& llvmContext,
                      std::vector<std::string> arguments, boost::string_view inputCCode,
                      std::unique_ptr<llvm::Module>* toFill);

}  // namespace chi

#endif  // CHI_C_COMPILER_HPP

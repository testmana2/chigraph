/// \file chi/Context.hpp
/// Defines the Context class and related functions

#ifndef CHI_CONTEXT_HPP
#define CHI_CONTEXT_HPP

#pragma once

#include <memory>
#include <unordered_map>

#include "chi/Fwd.hpp"
#include "chi/ModuleCache.hpp"
#include "chi/Support/Flags.hpp"
#include "chi/Support/json.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/CodeGen.h>  // for CodeGenOpt

#include <boost/filesystem/path.hpp>
#include <boost/utility/string_view.hpp>

namespace chi {

/// Settings for compiling modules
enum class CompileSettings {

	/// Use the cache in lib
	UseCache = 1u,

	/// Link in dependencies
	/// If this is set, it will be a ready to run module
	/// If not, it'll contain forward declarations for dependencies and full definitons
	/// For functions in that module
	LinkDependencies = 1u << 1,

	/// Default, which is both
	Default = UseCache | LinkDependencies
};

/// The class that handles the loading, creation, storing, and compilation of modules
/// It also stores a \c LLVMContext object to be used everywhere.
///
/// It stores all the modules and allows for compilation of them. Basic use looks like this:
///
/// ```
/// chi::Context ctx{"/path/to/workspace"};
///
/// chi::Reuslt res = ctx.loadModule("github.com/russelltg/hellochigraph");
/// if (!res) {
///     std::cerr << res << std::endl;
///     return;
/// }
/// ```
struct Context {
	/// Creates a context with just the lang module
	/// \param workPath Path to the workspace, or a subdirectory of the workspace
	Context(const boost::filesystem::path& workPath = {});

	/// Destructor
	~Context();

	// no move or copy, doesn't make sense
	Context(const Context& context) = delete;
	Context(Context&&)              = delete;

	/// Gets the module by the full name
	/// \param fullModuleName The name of the module to find
	/// \return ret_module The module that has the full name \c fullModuleName, nullptr if none were
	/// found
	ChiModule* moduleByFullName(const boost::filesystem::path& fullModuleName) const noexcept;

	/// Create a new GraphModule with the given full name
	/// \param fullName The new name
	/// \return The created GraphModule
	GraphModule* newGraphModule(const boost::filesystem::path& fullName);

	/// Get the list of modules in the workspace
	/// \return The module list
	// init it (pretty sure it inits windows networking stuff)
	std::vector<std::string> listModulesInWorkspace() const noexcept;

	/// Load a module from disk, also loads dependencies
	/// \param[in] name The name of the moudle
	/// \pre `!name.empty()`
	/// to fetch all dependencies as well. Leave as default to only use local modules.
	/// \param[out] toFill The module that was loaded, optional
	/// \return The result
	Result loadModule(const boost::filesystem::path& name, ChiModule** toFill = nullptr);

	/// Load a module from JSON -- avoid this use the string overload
	/// \param[in] fullName The full path of the module, including URL
	/// \param[in] json The JSON data
	/// \param[out] toFill The GraphModule* to fill into, optional
	/// \return The Result
	Result addModuleFromJson(const boost::filesystem::path& fullName, const nlohmann::json& json,
	                         GraphModule** toFill = nullptr);

	/// Adds a custom module to the Context
	/// \param modToAdd The module to add. The context will take excluseive ownership of it.
	/// \return True if the module was added (it didn't exist before)
	bool addModule(std::unique_ptr<ChiModule> modToAdd) noexcept;

	/// Unloads a module
	/// \param fullName The full name of the module to unload
	/// \return True if a module was unloaded
	bool unloadModule(const boost::filesystem::path& fullName);

	/// Gets a DataType from a module
	/// \param[in] module The full name of the module
	/// \param[in] name The name of the type, required
	/// \param[out] toFill The type to fill
	/// \pre `toFill != nullptr`
	/// \return The result
	Result typeFromModule(const boost::filesystem::path& module, boost::string_view name,
	                      DataType* toFill) noexcept;

	/// Gets a NodeType from the JSON and name
	/// \param[in] moduleName The full module name.
	/// \param[in] typeName The name of the node type
	/// \param[in] data The JSON data that is used to construct the NodeType.
	/// \param[out] toFill The point to fill
	/// \pre `toFill != nullptr` (the value the `unique_ptr` points to be can be null, but not the
	/// pointer to the `unique_ptr`)
	/// \return The Result
	Result nodeTypeFromModule(const boost::filesystem::path& moduleName,
	                          boost::string_view typeName, const nlohmann::json& data,
	                          std::unique_ptr<NodeType>* toFill) noexcept;
							  
	/// Create a converter node
	/// \param[in] fromType The type to convert from
	/// \param[in] toType The type to convert to
	/// \return The node type, or nullptr
	std::unique_ptr<NodeType> createConverterNodeType(const DataType& fromType, const DataType& toType);

	/// Get the workspace path of the Context
	/// \return The workspace path
	boost::filesystem::path workspacePath() const { return mWorkspacePath; }
	/// Check if this context has a workspace bound to it -- same as !workspacePath().empty()
	/// \return If it has a workspace
	bool hasWorkspace() const noexcept { return !workspacePath().empty(); }

	/// Compile a module to a \c llvm::Module
	/// \param[in] fullName The full name of the module to compile.
	/// If `moduleByFullName(fullName) == nullptr`, this function has no side-effects
	/// \param[in] settings The settings. See CompileSettings for more info
	/// \param[out] toFill The \c llvm::Module to fill -- this can be nullptr it will be replaced
	/// \pre toFill isn't null (the value the unique_ptr points to be can be null, but not the
	/// pointer to the unique_ptr)
	/// \return The `Result`
	Result compileModule(const boost::filesystem::path& fullName, Flags<CompileSettings> settings,
	                     std::unique_ptr<llvm::Module>* toFill);

	/// Compile a module to a \c llvm::Module
	/// \param[in] mod The module to compile
	/// \param[in] settings The settings. See CompileSettings for more details
	/// \param[out] toFill The \c llvm::Module to fill -- this can be nullptr it will be replaced
	/// \pre `toFill != nullptr` (the value the `unique_ptr` points to be can be null, but not the
	/// pointer to the `unique_ptr`)
	/// \return The `Result`
	Result compileModule(ChiModule& mod, Flags<CompileSettings> settings,
	                     std::unique_ptr<llvm::Module>* toFill);

	/// Find all uses of a node type in all the loaded modules
	/// \param moduleName The name of the module that the type being search for is in
	/// \param typeName The name of the type in `module` to search for
	/// \return All the `NodeInstance`s that are of that type
	std::vector<NodeInstance*> findInstancesOfType(const boost::filesystem::path& moduleName,
	                                               boost::string_view             typeName) const;

	/// Get the `LLVMContext`
	/// \return The `LLVMContext`
	llvm::LLVMContext& llvmContext() { return mLLVMContext; }

	/// Get the `LangModule`, if it has been loaded
	/// \return The `LangModule`
	LangModule* langModule() const { return mLangModule; }

	/// Get the modules in the Context
	/// \return The modules
	std::vector<ChiModule*> modules() const {
		std::vector<ChiModule*> ret;

		for (const auto& mod : mModules) { ret.push_back(mod.get()); }

		return ret;
	}

	/// Get the module cache
	/// \return The ModuleCache
	const ModuleCache& moduleCache() const { return *mModuleCache; }

	/// \copydoc Context::moduleCache
	ModuleCache& moduleCache() { return *mModuleCache; }

	/// Set the module cache
	/// \param newCache The new module cache
	/// \pre `newCache != nullptr`
	void setModuleCache(std::unique_ptr<ModuleCache> newCache);

private:
	boost::filesystem::path mWorkspacePath;

	llvm::LLVMContext mLLVMContext;

	std::vector<std::unique_ptr<ChiModule>> mModules;

	// This cache is only for use during compilation to not duplicate modules
	std::unordered_map<std::string /*full name*/, llvm::Module* /*the compiled module*/>
	    mCompileCache;

	LangModule* mLangModule = nullptr;

	std::unique_ptr<ModuleCache> mModuleCache;
	
	std::unordered_map<std::string /*from Type*/, std::unordered_map<std::string /*to type*/, std::unique_ptr<NodeType>>> mTypeConverters;
};

/// Get the workspace directory from a child of the workspace directory
/// Example: say you have a workspace at ~/chi/
/// If you used this with ~/chi/src/ it would return ~/chi/
/// \param path The child path
/// \return The workspace path, or an empty path if it wasn't found
boost::filesystem::path workspaceFromChildPath(const boost::filesystem::path& path);

/// Turns a type into a string
/// \param ty The type to stringify
/// \return The return string
std::string stringifyLLVMType(llvm::Type* ty);

/// Interpret LLVM IR, just a convenience function
/// \param[in] mod The LLVM Module to interpret
/// \param[in] optLevel How much the optimization should be applied. The default is roughly
/// equilivant to -O2
/// \param[in] args The arguments to pass to the function, empty by default
/// \param[in] funcToRun The function to run. By default it uses "main".
/// \param[out] ret The `GenericValue` to fill with the result of the function. Optional
/// \return The Result
Result interpretLLVMIR(std::unique_ptr<llvm::Module>          mod,
                       llvm::CodeGenOpt::Level                optLevel = llvm::CodeGenOpt::Default,
                       const std::vector<llvm::GenericValue>& args     = {},
                       llvm::Function* funcToRun = nullptr, llvm::GenericValue* ret = nullptr);

/// Interpret LLVM IR as if it were the main function
/// \param[in] mod The module to interpret
/// \param[in] optLevel The optimization level
/// \param[in] args The arguments to main
/// \param[in] funcToRun The function, defaults to "main" from `mod`
/// \param[out] ret The return from main. Optional.
/// \return The Result
Result interpretLLVMIRAsMain(std::unique_ptr<llvm::Module>   mod,
                             llvm::CodeGenOpt::Level         optLevel = llvm::CodeGenOpt::Default,
                             const std::vector<std::string>& args     = {},
                             llvm::Function* funcToRun = nullptr, int* ret = nullptr);
}  // namespace chi

#endif  // CHI_CONTEXT_HPP

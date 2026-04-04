use crate::asynccmd::{AsyncCmd, SpawnedAsyncCmd};
use crate::message::buildresult::BuildStatus;

use std::fmt;
use std::fs;
use std::io::{BufRead, BufReader, Seek, SeekFrom};
use std::path::Path;
use std::process::{Command, Stdio};

use tempfile::tempfile;

/// Identifies which build system a project uses.
#[derive(Clone, Debug, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum BuildSystem {
    CMake,
    Meson,
    Autotools,
    Cargo,
    Gradle,
    Make,
    Custom { command: String },
}

impl fmt::Display for BuildSystem {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            BuildSystem::CMake => write!(f, "cmake"),
            BuildSystem::Meson => write!(f, "meson"),
            BuildSystem::Autotools => write!(f, "autotools"),
            BuildSystem::Cargo => write!(f, "cargo"),
            BuildSystem::Gradle => write!(f, "gradle"),
            BuildSystem::Make => write!(f, "make"),
            BuildSystem::Custom { command } => write!(f, "custom({command})"),
        }
    }
}

/// Project-specific build configuration.
#[derive(Clone, Debug, serde::Serialize, serde::Deserialize)]
pub struct ProjectBuildConfig {
    pub name: String,
    pub path: String,
    pub build_system: BuildSystem,
    pub build_timeout_seconds: u16,
    pub configure_args: Vec<String>,
    pub build_args: Vec<String>,
    pub test_command: Option<Vec<String>>,
}

/// The build executor — replaces ofborg's Nix struct.
#[derive(Clone, Debug)]
pub struct BuildExecutor {
    pub build_timeout: u16,
}

impl BuildExecutor {
    pub fn new(build_timeout: u16) -> Self {
        Self { build_timeout }
    }

    /// Build a project using its configured build system.
    pub fn build_project(
        &self,
        project_root: &Path,
        config: &ProjectBuildConfig,
    ) -> Result<fs::File, fs::File> {
        let project_dir = project_root.join(&config.path);
        let cmd = self.build_command(&project_dir, config);
        self.run(cmd, true)
    }

    /// Build a project asynchronously.
    pub fn build_project_async(
        &self,
        project_root: &Path,
        config: &ProjectBuildConfig,
    ) -> SpawnedAsyncCmd {
        let project_dir = project_root.join(&config.path);
        let cmd = self.build_command(&project_dir, config);
        AsyncCmd::new(cmd).spawn()
    }

    /// Run tests for a project.
    pub fn test_project(
        &self,
        project_root: &Path,
        config: &ProjectBuildConfig,
    ) -> Result<fs::File, fs::File> {
        let project_dir = project_root.join(&config.path);
        let cmd = self.test_command(&project_dir, config);
        self.run(cmd, true)
    }

    fn build_command(&self, project_dir: &Path, config: &ProjectBuildConfig) -> Command {
        match &config.build_system {
            BuildSystem::CMake => {
                let build_dir = project_dir.join("build");
                let mut cmd = Command::new("cmake");
                cmd.arg("--build").arg(&build_dir);
                cmd.args(["--config", "Release"]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Meson => {
                let mut cmd = Command::new("meson");
                cmd.arg("compile");
                cmd.args(["-C", "build"]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Autotools => {
                let mut cmd = Command::new("make");
                cmd.args(["-j", &num_cpus().to_string()]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Cargo => {
                let mut cmd = Command::new("cargo");
                cmd.arg("build").arg("--release");
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Gradle => {
                let gradlew = project_dir.join("gradlew");
                let prog = if gradlew.exists() {
                    gradlew.to_string_lossy().to_string()
                } else {
                    "gradle".to_string()
                };
                let mut cmd = Command::new(prog);
                cmd.arg("build");
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Make => {
                let mut cmd = Command::new("make");
                cmd.args(["-j", &num_cpus().to_string()]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Custom { command } => {
                let mut cmd = Command::new("sh");
                cmd.args(["-c", command]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
        }
    }

    fn test_command(&self, project_dir: &Path, config: &ProjectBuildConfig) -> Command {
        if let Some(ref test_cmd) = config.test_command {
            let mut cmd = Command::new(&test_cmd[0]);
            for arg in &test_cmd[1..] {
                cmd.arg(arg);
            }
            cmd.current_dir(project_dir);
            return cmd;
        }

        match &config.build_system {
            BuildSystem::CMake => {
                let mut cmd = Command::new("ctest");
                cmd.arg("--test-dir").arg("build");
                cmd.args(["--output-on-failure"]);
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Meson => {
                let mut cmd = Command::new("meson");
                cmd.arg("test").args(["-C", "build"]);
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Autotools | BuildSystem::Make => {
                let mut cmd = Command::new("make");
                cmd.arg("check");
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Cargo => {
                let mut cmd = Command::new("cargo");
                cmd.arg("test").arg("--release");
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Gradle => {
                let gradlew = project_dir.join("gradlew");
                let prog = if gradlew.exists() {
                    gradlew.to_string_lossy().to_string()
                } else {
                    "gradle".to_string()
                };
                let mut cmd = Command::new(prog);
                cmd.arg("test");
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Custom { command } => {
                let mut cmd = Command::new("sh");
                cmd.args(["-c", command]);
                cmd.current_dir(project_dir);
                cmd
            }
        }
    }

    pub fn run(&self, mut cmd: Command, keep_stdout: bool) -> Result<fs::File, fs::File> {
        let stderr = tempfile().expect("Fetching a stderr tempfile");
        let mut reader = stderr.try_clone().expect("Cloning stderr to the reader");

        let stdout: Stdio = if keep_stdout {
            Stdio::from(stderr.try_clone().expect("Cloning stderr for stdout"))
        } else {
            Stdio::null()
        };

        let status = cmd
            .stdout(stdout)
            .stderr(Stdio::from(stderr))
            .status()
            .expect("Running build command ...");

        reader
            .seek(SeekFrom::Start(0))
            .expect("Seeking to Start(0)");

        if status.success() {
            Ok(reader)
        } else {
            Err(reader)
        }
    }
}

fn num_cpus() -> usize {
    std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(4)
}

pub fn lines_from_file(file: fs::File) -> Vec<String> {
    BufReader::new(file)
        .lines()
        .map_while(Result::ok)
        .collect()
}

pub fn wait_for_build_status(spawned: SpawnedAsyncCmd) -> BuildStatus {
    match spawned.wait() {
        Ok(s) => match s.code() {
            Some(0) => BuildStatus::Success,
            Some(_code) => BuildStatus::Failure,
            None => BuildStatus::UnexpectedError {
                err: "process terminated by signal".into(),
            },
        },
        Err(err) => BuildStatus::UnexpectedError {
            err: format!("failed on interior command {err}"),
        },
    }
}

/// Known projects in the Project Tick monorepo.
pub fn known_projects() -> Vec<ProjectBuildConfig> {
    vec![
        ProjectBuildConfig {
            name: "mnv".into(),
            path: "mnv".into(),
            build_system: BuildSystem::Autotools,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: Some(vec!["make".into(), "check".into()]),
        },
        ProjectBuildConfig {
            name: "cgit".into(),
            path: "cgit".into(),
            build_system: BuildSystem::Make,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "cmark".into(),
            path: "cmark".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "neozip".into(),
            path: "neozip".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "genqrcode".into(),
            path: "genqrcode".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "json4cpp".into(),
            path: "json4cpp".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "tomlplusplus".into(),
            path: "tomlplusplus".into(),
            build_system: BuildSystem::Meson,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "libnbtplusplus".into(),
            path: "libnbtplusplus".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "meshmc".into(),
            path: "meshmc".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 3600,
            configure_args: vec![],
            build_args: vec!["--config".into(), "Release".into()],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "forgewrapper".into(),
            path: "forgewrapper".into(),
            build_system: BuildSystem::Gradle,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "corebinutils".into(),
            path: "corebinutils".into(),
            build_system: BuildSystem::Make,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
    ]
}

/// Look up a project by name.
pub fn find_project(name: &str) -> Option<ProjectBuildConfig> {
    known_projects().into_iter().find(|p| p.name == name)
}

/// Detect which projects changed based on modified file paths.
pub fn detect_changed_projects(changed_files: &[String]) -> Vec<String> {
    let projects = known_projects();
    let mut changed = Vec::new();

    for project in &projects {
        let prefix = format!("{}/", project.path);
        if changed_files.iter().any(|f| f.starts_with(&prefix)) {
            changed.push(project.name.clone());
        }
    }

    changed.sort();
    changed.dedup();
    changed
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_detect_changed_projects() {
        let files = vec![
            "mnv/src/main.c".into(),
            "mnv/Makefile.am".into(),
            "meshmc/CMakeLists.txt".into(),
            "README.md".into(),
        ];
        let changed = detect_changed_projects(&files);
        assert_eq!(changed, vec!["meshmc", "mnv"]);
    }

    #[test]
    fn test_detect_changed_projects_none() {
        let files = vec!["README.md".into(), ".gitignore".into()];
        let changed = detect_changed_projects(&files);
        assert!(changed.is_empty());
    }

    #[test]
    fn test_find_project() {
        assert!(find_project("meshmc").is_some());
        assert!(find_project("nonexistent").is_none());
        assert_eq!(
            find_project("meshmc").unwrap().build_system,
            BuildSystem::CMake
        );
        assert_eq!(
            find_project("forgewrapper").unwrap().build_system,
            BuildSystem::Gradle
        );
    }

    #[test]
    fn test_build_system_display() {
        assert_eq!(BuildSystem::CMake.to_string(), "cmake");
        assert_eq!(BuildSystem::Meson.to_string(), "meson");
        assert_eq!(BuildSystem::Cargo.to_string(), "cargo");
        assert_eq!(BuildSystem::Gradle.to_string(), "gradle");
    }
}

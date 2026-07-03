use serde::{Deserialize, Serialize};
use std::path::PathBuf;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum ProjectStatus {
    Active,
    Idle,
    Maintenance,
    Archived,
}

impl Default for ProjectStatus {
    fn default() -> Self {
        Self::Idle
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Project {
    pub name: String,
    pub path: PathBuf,
    pub status: ProjectStatus,
    pub description: Option<String>,
    pub links: Vec<String>,
    pub ssh_endpoint: Option<String>,
}

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct AppConfig {
    pub projects: Vec<Project>,
    pub last_selected: Option<String>,
}

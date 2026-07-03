use crate::models::{AppConfig, Project};
use std::fs;
use std::path::PathBuf;
use anyhow::{Context, Result};

pub struct App {
    pub config: AppConfig,
    pub selected_index: usize,
    pub should_quit: bool,
    pub config_path: PathBuf,
}

impl App {
    pub fn new() -> Result<Self> {
        let home = std::env::var("HOME").context("HOME env var not set")?;
        let config_path = PathBuf::from(home).join(".dev-flow.json");
        
        let config = if config_path.exists() {
            let content = fs::read_to_string(&config_path)?;
            serde_json::from_str(&content).unwrap_or_default()
        } else {
            AppConfig::default()
        };

        let selected_index = if let Some(last) = &config.last_selected {
            config.projects.iter().position(|p| &p.name == last).unwrap_or(0)
        } else {
            0
        };

        Ok(Self {
            config,
            selected_index,
            should_quit: false,
            config_path,
        })
    }

    pub fn next(&mut self) {
        if !self.config.projects.is_empty() {
            self.selected_index = (self.selected_index + 1) % self.config.projects.len();
        }
    }

    pub fn previous(&mut self) {
        if !self.config.projects.is_empty() {
            if self.selected_index > 0 {
                self.selected_index -= 1;
            } else {
                self.selected_index = self.config.projects.len() - 1;
            }
        }
    }

    pub fn current_project(&self) -> Option<&Project> {
        self.config.projects.get(self.selected_index)
    }

    pub fn save(&mut self) -> Result<()> {
        if let Some(project) = self.current_project() {
            self.config.last_selected = Some(project.name.clone());
        }
        let content = serde_json::to_string_pretty(&self.config)?;
        fs::write(&self.config_path, content)?;
        Ok(())
    }

    pub fn trigger_action(&self) -> Result<()> {
        if let Some(project) = self.current_project() {
            let action_path = "/tmp/dev_flow_action";
            let action = format!("cd {}", project.path.to_string_lossy());
            fs::write(action_path, action).context("Failed to write to IPC file")?;
        }
        Ok(())
    }
}

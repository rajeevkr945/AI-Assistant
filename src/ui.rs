use ratatui::{
    layout::{Constraint, Direction, Layout},
    style::{Color, Modifier, Style},
    text::{Line, Span},
    widgets::{Block, Borders, List, ListItem, ListState, Paragraph, Wrap},
    Frame,
};
use crate::app::App;
use crate::models::ProjectStatus;

pub fn draw(f: &mut Frame, app: &mut App) {
    let chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(40), Constraint::Percentage(60)].as_ref())
        .split(f.size());

    // --- Left Column: Project List ---
    let items: Vec<ListItem> = app
        .config
        .projects
        .iter()
        .map(|p| {
            let color = match p.status {
                ProjectStatus::Active => Color::Green,
                ProjectStatus::Idle => Color::White,
                ProjectStatus::Maintenance => Color::Yellow,
                ProjectStatus::Archived => Color::DarkGray,
            };
            ListItem::new(Span::styled(
                format!(" > {}", p.name),
                Style::default().fg(color),
            ))
        })
        .collect();

    let mut list_state = ListState::default();
    list_state.select(Some(app.selected_index));

    let list = List::new(items)
        .block(Block::default().borders(Borders::ALL).title(" Projects "))
        .highlight_style(
            Style::default()
                .bg(Color::Blue)
                .fg(Color::White)
                .add_modifier(Modifier::BOLD),
        )
        .highlight_symbol(">> ");

    f.render_stateful_widget(list, chunks[0], &mut list_state);

    // --- Right Column: Metadata ---
    if let Some(project) = app.current_project() {
        let mut details = vec![
            Line::from(vec![
                Span::styled("Name:        ", Style::default().add_modifier(Modifier::BOLD)),
                Span::raw(&project.name),
            ]),
            Line::from(vec![
                Span::styled("Path:        ", Style::default().add_modifier(Modifier::BOLD)),
                Span::styled(project.path.to_string_lossy(), Style::default().fg(Color::Cyan)),
            ]),
            Line::from(vec![
                Span::styled("Status:      ", Style::default().add_modifier(Modifier::BOLD)),
                Span::raw(format!("{:?}", project.status)),
            ]),
        ];

        if let Some(desc) = &project.description {
            details.push(Line::from(""));
            details.push(Line::from(Span::styled("Description:", Style::default().add_modifier(Modifier::BOLD))));
            details.push(Line::from(Span::raw(desc)));
        }

        if let Some(ssh) = &project.ssh_endpoint {
            details.push(Line::from(""));
            details.push(Line::from(vec![
                Span::styled("SSH:         ", Style::default().add_modifier(Modifier::BOLD)),
                Span::styled(ssh, Style::default().fg(Color::Magenta)),
            ]));
        }

        if !project.links.is_empty() {
            details.push(Line::from(""));
            details.push(Line::from(Span::styled("Links:", Style::default().add_modifier(Modifier::BOLD))));
            for link in &project.links {
                details.push(Line::from(Span::styled(format!(" - {}", link), Style::default().fg(Color::Blue))));
            }
        }

        let metadata = Paragraph::new(details)
            .block(Block::default().borders(Borders::ALL).title(" Metadata "))
            .wrap(Wrap { trim: true });

        f.render_widget(metadata, chunks[1]);
    } else {
        let empty = Paragraph::new("No projects found. Add one to ~/.dev-flow.json")
            .block(Block::default().borders(Borders::ALL).title(" Metadata "));
        f.render_widget(empty, chunks[1]);
    }
}

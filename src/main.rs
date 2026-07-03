use dev_flow::app::App;
use anyhow::Result;

fn main() -> Result<()> {
    let mut app = App::new()?;
    println!("Loaded {} projects", app.config.projects.len());
    
    // For now, just a demo: save and exit
    app.save()?;
    println!("App state saved to {:?}", app.config_path);
    
    Ok(())
}

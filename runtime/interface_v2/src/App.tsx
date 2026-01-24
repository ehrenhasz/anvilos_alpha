
import { TerminalComponent } from './components/TerminalComponent';

function App() {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100vh' }}>
      <header style={{ padding: '10px', borderBottom: '1px solid #333', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <span style={{ fontWeight: 'bold', color: '#00ff00' }}>GEMINI CLI V2 (INTERCEPTOR)</span>
        <span style={{ fontSize: '0.8em', color: '#666' }}>PROTOCOL: RFC-0051</span>
      </header>
      <div style={{ flex: 1, overflow: 'hidden', padding: '10px' }}>
        <TerminalComponent />
      </div>
    </div>
  );
}

export default App;

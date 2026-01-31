import { Sidebar } from './components/Sidebar';
import { TerminalComponent } from './components/TerminalComponent';
import './index.css';

function App() {
  return (
    <div style={{ display: 'flex', width: '100vw', height: '100vh', overflow: 'hidden' }}>
      <Sidebar />
      <div style={{ flex: 1, backgroundColor: '#121212', padding: '10px' }}>
         <TerminalComponent />
      </div>
    </div>
  );
}

export default App;
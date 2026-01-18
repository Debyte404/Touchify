import { useState, useEffect } from 'react'
import './index.css'

function App() {
  const [students, setStudents] = useState([]);
  const [connected, setConnected] = useState(false);
  const [lastUpdateId, setLastUpdateId] = useState(null);

  useEffect(() => {
    // Fetch initial student list
    fetch('http://127.0.0.1:8000/students')
      .then(res => res.json())
      .then(data => setStudents(data))
      .catch(err => console.error('Error fetching students:', err));
  }, []);

  useEffect(() => {
    let ws;
    let reconnectTimeout;

    const connect = () => {
      ws = new WebSocket('ws://127.0.0.1:8000/ws/attendance');

      ws.onopen = () => {
        console.log('Connected to Gateway WebSocket');
        setConnected(true);
      };

      ws.onmessage = (event) => {
        const message = JSON.parse(event.data);
        if (message.type === 'biometric_event') {
          const { finger_id } = message.data;
          const timestamp = message.timestamp;
          
          setStudents(prev => prev.map(student => {
            if (student.id === finger_id) {
              setLastUpdateId(finger_id);
              setTimeout(() => setLastUpdateId(null), 2000);
              
              return {
                ...student,
                status: "Present",
                time: timestamp.split(' ')[1]
              };
            }
            return student;
          }));
        }
      };

      ws.onclose = () => {
        console.log('Disconnected from Gateway WebSocket. Retrying...');
        setConnected(false);
        reconnectTimeout = setTimeout(connect, 2000);
      };

      ws.onerror = (err) => {
        console.error('WebSocket error:', err);
        ws.close();
      };
    };

    connect();

    return () => {
      if (ws) ws.close();
      clearTimeout(reconnectTimeout);
    };
  }, []);

  return (
    <div className="dashboard">
      <header>
        <div>
          <h1>Touchify Dash</h1>
          <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem' }}>Real-time Biometric Attendance System</p>
        </div>
        
        <div className="status-badge">
          <div className={`status-dot ${connected ? 'connected' : ''}`}></div>
          {connected ? 'GATEWAY LIVE' : 'GATEWAY OFFLINE'}
        </div>
      </header>

      <div className="main-card">
        <table>
          <thead>
            <tr>
              <th>Roll Number</th>
              <th>Student Name</th>
              <th>Year</th>
              <th>Section</th>
              <th>Status</th>
              <th>Time</th>
            </tr>
          </thead>
          <tbody>
            {students.map((student) => (
              <tr 
                key={student.id} 
                className={lastUpdateId === student.id ? 'update-flash' : ''}
              >
                <td className="student-id">{student.id}</td>
                <td className="student-name">{student.name}</td>
                <td>{student.class}</td>
                <td>{student.section}</td>
                <td>
                  <div className={`attendance-status ${student.status === 'Present' ? 'status-present' : 'status-absent'}`}>
                    <div className={`status-dot ${student.status === 'Present' ? 'connected pulse' : ''}`} 
                         style={student.status === 'Absent' ? { background: '#475569', boxShadow: 'none' } : {}}></div>
                    {student.status.toUpperCase()}
                  </div>
                </td>
                <td style={{ color: 'var(--text-muted)', fontWeight: '500' }}>{student.time}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <footer style={{ textAlign: 'center', padding: '1rem', color: 'var(--text-muted)', fontSize: '0.8rem' }}>
        &copy; 2026 Touchify Project &bull; Powered by FastAPI & ESP32
      </footer>
    </div>
  )
}

export default App

import { Router } from 'preact-router'

import Main from '../routes/main';
import Setup from '../routes/setup';

const App = () => (
	<div id="app">
		<Router>
			<Main path="/" />
			<Setup path="/setup" />
		</Router>
	</div>
);

export default App;

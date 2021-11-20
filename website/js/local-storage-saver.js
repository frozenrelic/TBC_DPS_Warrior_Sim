/**
 * Renders a save dialog that presents an input to name the set and a button to execute saving
 */
function renderSave() {
    const elementContent = `
        <label>Enter a name</label>
        <input id="saving-name">
        <div class="saving-controls">
            <button onclick="saveToLocalStorage()">Save</button>
            <button onclick="removeAll()">Cancel</button>
        </div>
    `;

    const element = document.createElement('div');
    element.className = 'saving-dialog';
    element.innerHTML = elementContent;

    document.body.append(element);
    applyBackdrop();
}

function applyBackdrop() {
    const backdrop = document.createElement('div');
    backdrop.className = 'saving-backdrop';
    document.body.append(backdrop);
}

function removeBackdrop() {
    document.querySelector('.saving-backdrop').remove();
}

function removeDialog() {
    document.querySelector('.saving-dialog').remove();
}

function removeAll() {
    removeBackdrop();
    removeDialog();
}

/**
 * Saves to local storage using primary file content generation method
 */
function saveToLocalStorage() {
    const setName = document.querySelector('#saving-name').value;
    const content = generateFileContents();

    for (let i = 0; i < 10; i++) {
        window.localStorage.setItem(setName + i, content);
    }

    window.localStorage.setItem(setName, content);
    removeAll();
}

/**
 * Renders a loading dialog that presents the user with a list of their saved configs
 * Each config is displayed as a clickable element that will immediately load on click
 */
function renderLoad() {
    const elementContent = `
        <label>Click the name you'd like to load</label>
        <input class="loading-dialog-search" placeholder="Search">
        
        <div class="loading-sets">
        </div>
        <button onclick="removeAll()">Cancel</button>
    `;

    const element = document.createElement('div');
    element.className = 'saving-dialog';
    element.innerHTML = elementContent;

    document.body.append(element);
    document.querySelector('.loading-dialog-search')
        .addEventListener('keyup', debounce(filterSets, 300));
    renderLoadingOptions(getSets());
    applyBackdrop();
}

function renderLoadingOptions(options) {
    const template = function(name) { return `<div onclick="loadFromLocalStorage('${name}')">${name}</div>`; };
    document.querySelector('.loading-sets').innerHTML =
        options.sort((a, b) => a > b)
            .reduce((accumulator, current) => `${accumulator} ${template(current)}`, '');
}

function filterSets() {
    const input = document.querySelector('.loading-dialog-search').value;
    const searchTerm = input.trim().toLowerCase();
    const filtered = getSets().filter(set => set.trim().toLowerCase().includes(searchTerm));
    renderLoadingOptions(filtered);
}

function getSets() {
    const store = window.localStorage;

    const sets = [];
    for (let i = 0; i < store.length; i++) {
        sets.push(store.key(i));
    }

    return sets;
}

/**
 * Loads a saved config from local storage by using primary loading function loadContents
 * @param set - name of set to load
 */
function loadFromLocalStorage(set) {
    const setContent = window.localStorage.getItem(set);
    loadContents(setContent);
    setTimeout(displayTalentBootup, 200);
    setTimeout(calcTotalTalents, 200);

    removeAll();
}

function debounce(func, delay) {
    let timer;
    return function() {
        let context = this;
        let args = arguments;
        clearTimeout(timer);
        timer = setTimeout(() => {
            func.apply(context, args);
        }, delay);
    }
}

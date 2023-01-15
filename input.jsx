const MyComp1 = () => (
    <div>
        <strong>MyComp1 title</strong>
        <p>MyComp1 text</p>
    </div>
);

const MyComp2 = () => (
    <MyComp1>
        <strong>MyComp2 title</strong>
        <p>MyComp2 text</p>
    </MyComp1>
);

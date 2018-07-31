function[] = writeFaustDSP(                     ...
                            faustDSPtarget,     ...
                            permission,         ...
                            precision,          ...
                            antialFIRs,         ...
                            sosData,            ...
                            alpha,              ...
                            minGain             ...
                            )

fileID = fopen(faustDSPtarget, permission);

fprintf(fileID, ...
            'fi = library("filters.lib");\nba = library("basics.lib");\n\n' ...
            );
            
fprintf(fileID,                                                         ...
            ['gainAdjust = alpha\n'                                     ...
            'with{\n'                                                   ...
            '    alpha= hslider("Gain Adjust [unit:dB][style:knob]",'   ...
            '0, -120, 120, 0.1) : ba.db2linear;\n'                      ...
            '};\n\n'                                                    ...
            ]                                                           ...
            );

nBranches = size(antialFIRs, 1);
            
for b = 1:nBranches
    
        fprintf(fileID, ...
                    ['antial' num2str(b) 'Taps = (']
                    );
                    
        for t = 1:length(antialFIRs{b, 1})
            
            fprintf(fileID, num2str(antialFIRs{b, 1}(t), precision));
        
            if isequal(t, length(antialFIRs{b, 1}))
                fprintf(fileID, ');\n');
            else
                fprintf(fileID, ', ');
            endif
        
        endfor
    
        fprintf(fileID,                 ...
                    [                   ...
                    'antial'            ...
                    num2str(b)          ...
                    ' = fi.fir(antial'  ...
                    num2str(b)          ...
                    'Taps);\n'          ...
                    ]                   ...
                    );
    
        fprintf(fileID,                                     ...
                    [                                       ...
                    'kernel'                                ...
                    num2str(b)                              ...
                    ' = *(_, '                              ...
                    num2str(sosData{b, 1}.G, precision)     ...
                    ') : '                                  ...
                    ]                                       ...
                    );
                    
        for s = 1:size(sosData{b, 1}.SOS, 1)
            
            fprintf(fileID, 'fi.tf2np(');
            
            for c = [1 2 3 5 6]
                
                fprintf(fileID, num2str(sosData{b, 1}.SOS(s, c), precision));
                
                if isequal(c, 6)
                    if isequal(s, size(sosData{b, 1}.SOS, 1))
                        fprintf(fileID, ');\n');
                    else
                        fprintf(fileID, ') : ');
                    endif
                else
                    fprintf(fileID, ', ');
                endif
                
            endfor
            
        endfor
        
        fprintf(fileID,                                             ...
                    ['branch' num2str(b) ' = '                      ...
                    'antial' num2str(b) ' : '                       ...
                    '^(_, ' num2str(b) ') : '                     ...
                    '*(_, ^(gainAdjust, ' num2str(1 - b) ')) : '  ...
                    'kernel' num2str(b) ';\n\n'                     ...
                    ]                                               ...
                    );
        
endfor

fprintf(fileID, 'process = _ <: ');

for b = 1:nBranches
    
    fprintf(fileID, ['branch' num2str(b)]);
    
    if isequal(b, nBranches)
        fprintf(fileID, ' :> _;\n');
    else
        fprintf(fileID, ', ');
    endif
    
endfor

fclose(fileID);

endfunction
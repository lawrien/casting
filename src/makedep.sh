for i in *.c                                                        
  do                                                                    
     cc -I../include -MM $i | sed 's/\.\.\/include\///g'                
     echo ""                                                            
  done   
